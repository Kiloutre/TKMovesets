#include "ImportProfile.hpp"
#include "Editor_t8Retail.hpp"
#include "RetailCommands.hpp"
#include "RetailNames.hpp"
#include "Helpers.hpp"
#include "Structs_t8.h"
#include <filesystem>
#include <format>
#include <fstream>
#include <numeric>

namespace R = RetailT8Snapshot;
namespace
{
template <class T> T At(std::span<const Byte> bytes, size_t offset)
{
    if (offset > bytes.size() || sizeof(T) > bytes.size() - offset)
        throw std::out_of_range("Retail record extent");
    T value;
    memcpy(&value, bytes.data() + offset, sizeof(value));
    return value;
}
template <class T> void Put(std::span<Byte> bytes, size_t offset, T value)
{
    if (offset > bytes.size() || sizeof(T) > bytes.size() - offset)
        throw std::out_of_range("Retail edit extent");
    memcpy(bytes.data() + offset, &value, sizeof(value));
}
size_t Align(size_t size)
{
    return (size + 7) & ~size_t(7);
}
} // namespace

std::unique_ptr<EditorLogic::MutationState> EditorT8Retail::CaptureMutationState()
{
    struct State final : MutationState
    {
        std::shared_ptr<const RetailEditorData::Metadata> &target;
        std::shared_ptr<const RetailEditorData::Metadata> saved;
        State(decltype(target) t) : target(t), saved(t)
        {
        }
        void Restore() noexcept override
        {
            target = saved;
        }
    };
    return std::make_unique<State>(m_metadata);
}
const std::string *EditorT8Retail::CustomName(uint32_t hash) const
{
    if (m_metadata)
    {
        auto found = m_metadata->names.find(hash);
        if (found != m_metadata->names.end())
            return &found->second;
    }
    return nullptr;
}
const std::string *EditorT8Retail::FileName(uint32_t hash) const
{
    if (m_metadata)
    {
        auto found = m_metadata->fileNames.find(hash);
        if (found != m_metadata->fileNames.end() && !found->second.empty())
            return &found->second;
    }
    return nullptr;
}
void EditorT8Retail::Publish(std::span<const Byte> payload, const RetailEditorData::Metadata &metadata)
{
    // The document's custom property list (import profile) is kept through structural edits.
    const auto properties = ImportProfile::ReadProperties({m_moveset, static_cast<size_t>(m_movesetSize)});
    auto file = RetailEditorData::Serialize(*m_header, payload, metadata, &properties);
    std::unique_ptr<Byte, decltype(&free)> next(AllocateMutationBuffer(file.size()), free);
    if (!next)
        throw std::bad_alloc();
    memcpy(next.get(), file.data(), file.size());
    auto previous = m_moveset;
    LoadMovesetPtr(next.get(), file.size());
    next.release();
    free(previous);
}

void EditorT8Retail::Rebuild(size_t table, const RecordEdit &edit, const std::vector<Byte> &replacement,
                             const std::vector<uint32_t> &origins)
{
    const auto stride = R::Tables.at(table).stride;
    if (replacement.size() != edit.sources.size() * stride || origins.size() != edit.sources.size())
        throw std::invalid_argument("Replacement rows differ");
    const auto newCount = Count(table) + edit.difference;
    if ((table == 12 && (!newCount || newCount >= 0x8000)) || newCount > INT_MAX)
        throw std::out_of_range("Retail table size cannot be represented");
    auto remap = [&](uint64_t id) {
        if (table == 12 && id >= edit.start && id < edit.start + edit.oldCount)
        {
            auto found = std::find(edit.retained.begin(), edit.retained.end(), int(id));
            if (found != edit.retained.end())
                return uint64_t(edit.start + (found - edit.retained.begin()));
        }
        return edit.Remap(id);
    };
    auto metadata = *m_metadata;
    for (auto &id : metadata.remaps[table])
        if (id != UINT32_MAX)
            id = static_cast<uint32_t>(remap(id));
    auto &provenance = metadata.origins[table];
    provenance.erase(provenance.begin() + edit.start, provenance.begin() + edit.start + edit.oldCount);
    provenance.insert(provenance.begin() + edit.start, origins.begin(), origins.end());
    std::array<std::vector<Byte>, 20> tables;
    auto entries = m_entries;
    for (size_t t = 0; t < 20; ++t)
    {
        auto bytes = R::Table({m_movesetData, static_cast<size_t>(m_movesetDataSize)}, t);
        tables[t].assign(bytes.begin(), bytes.end());
        if (t == table)
        {
            auto &b = tables[t];
            b.erase(b.begin() + edit.start * stride, b.begin() + (edit.start + edit.oldCount) * stride);
            b.insert(b.begin() + edit.start * stride, replacement.begin(), replacement.end());
        }
        entries[t].count = tables[t].size() / entries[t].stride;
        // File-local virtual addresses make resized tables disjoint. Provenance
        // retains native identities; import relocates these typed addresses.
        entries[t].address = entries[t].count ? 0x600000000000ULL + (uint64_t(t) << 32) : 0;
    }
    for (size_t t = 0; t < 20; ++t)
        for (size_t row = 0; row < entries[t].count; ++row)
        {
            auto record = std::span<Byte>(tables[t]).subspan(row * entries[t].stride, entries[t].stride);
            for (const auto &field : Schema(t))
                if (field.kind == Kind::Reference)
                {
                    if (t == 12 && (field.offset == 0xa0 || field.offset == 0xb0 || field.offset == 0xc0) &&
                        At<int32_t>(record, field.offset + 8) <= 0)
                        continue;
                    const auto &old = m_entries[field.target];
                    auto pointer = At<uint64_t>(record, field.offset);
                    if (!pointer || pointer < old.address || pointer - old.address >= old.count * old.stride ||
                        (pointer - old.address) % old.stride)
                        continue;
                    auto id = (pointer - old.address) / old.stride;
                    if (field.target == table)
                    {
                        if ((t == 14 && table == 15) || (t == 4 && table == 5))
                        {
                            auto count = t == 14 ? uint64_t(At<uint16_t>(record, 2)) : At<uint32_t>(record, 4);
                            auto range = edit.RemapRange(id, count);
                            id = range.first;
                            if (t == 14)
                            {
                                if (range.second > UINT16_MAX)
                                    throw std::out_of_range("Input sequence is too long");
                                Put(record, 2, uint16_t(range.second));
                            }
                            else
                                Put(record, 4, uint32_t(range.second));
                            if (!range.second)
                            {
                                Put(record, field.offset, uint64_t(0));
                                continue;
                            }
                        }
                        else
                            id = edit.Remap(id);
                    }
                    Put(record, field.offset,
                        id == UINT64_MAX ? uint64_t(0) : entries[field.target].address + id * old.stride);
                }
            auto remapMove = [&](size_t offset) {
                auto id = At<uint16_t>(record, offset);
                if (id < m_entries[12].count)
                {
                    auto mapped = remap(id);
                    if (mapped == UINT64_MAX || mapped >= 0x8000)
                        throw std::out_of_range("Invalid moved record reference");
                    Put(record, offset, uint16_t(mapped));
                }
            };
            if (table == 12)
            {
                if (t == 12)
                    remapMove(0xcc);
                if ((t == 6 || t == 7) && At<uint64_t>(record, 0) != RetailT8Commands::GroupCancel)
                    remapMove(36);
                // Reaction move ids are 0x50..0x6A only; 0x4E is downed_rotation (reaction picker 0x1418FE330).
                if (t == 0)
                    for (size_t i = 0; i < std::size(StructsT8::Reactions{}.moveid); ++i)
                        remapMove(offsetof(StructsT8::Reactions, moveid) + i * sizeof(uint16_t));
            }
            if (table == 14 && (t == 6 || t == 7))
            {
                auto command = At<uint64_t>(record, 0);
                if (RetailT8Commands::IsInputSequence(command) && RetailT8Commands::InputSequenceId(command) < edit.total)
                {
                    auto id = edit.Remap(RetailT8Commands::InputSequenceId(command));
                    if (id >= 0x10000 - RetailT8Commands::InputSequenceBase)
                        throw std::out_of_range("Referenced input sequence was removed");
                    Put(record, 0, (command & 0xffffffff00000000ULL) | (RetailT8Commands::InputSequenceBase + id));
                }
            }
            if ((table == 3 || table == 18) && (t == 9 || t == 10 || t == 11))
            {
                const size_t offset = t == 9 ? 16 : 8;
                auto property = At<uint32_t>(record, offset);
                if ((table == 3 && property == 33403) || (table == 18 && property == 34447))
                {
                    auto id = At<uint32_t>(record, offset + 4);
                    if (id < edit.total)
                        Put(record, offset + 4, uint32_t(edit.Remap(id)));
                }
            }
            if (table == 7 && t == 6 && At<uint64_t>(record, 0) == RetailT8Commands::GroupCancel)
            {
                auto id = At<uint16_t>(record, 36);
                if (id < m_entries[7].count)
                {
                    auto mapped = edit.Remap(id);
                    if (mapped > UINT16_MAX)
                        throw std::out_of_range("Grouped cancel reference cannot be represented");
                    Put(record, 36, uint16_t(mapped));
                }
            }
        }
    auto payload = std::span<const Byte>(m_movesetData, static_cast<size_t>(m_movesetDataSize));
    auto header = At<R::Header>(payload, 0);
    std::vector<Byte> result(payload.begin(), payload.begin() + header.directoryOffset + 20 * sizeof(R::Entry));
    if (table == 12)
        for (size_t offset = 0x30; offset < 0x168; offset += 2)
        {
            auto id = At<uint16_t>(result, header.sourceHeaderOffset + offset);
            if (id < m_entries[12].count)
                Put(std::span<Byte>(result), header.sourceHeaderOffset + offset, uint16_t(remap(id)));
        }
    for (size_t t = 0; t < 20; ++t)
    {
        entries[t].offset = Align(result.size());
        result.resize(entries[t].offset);
        result.insert(result.end(), tables[t].begin(), tables[t].end());
        Put(std::span<Byte>(result), header.directoryOffset + t * sizeof(R::Entry), entries[t]);
        Put(std::span<Byte>(result), header.sourceHeaderOffset + R::Tables[t].pointerOffset, entries[t].address);
        Put(std::span<Byte>(result), header.sourceHeaderOffset + R::Tables[t].countOffset, entries[t].count);
    }
    if (header.version >= 2)
    {
        auto assets = R::Assets(payload);
        const auto directoryOffset = Align(result.size());
        R::AssetDirectory directory{
            {'T', '8', 'A', 'N'}, uint32_t(assets.size()), directoryOffset + sizeof(R::AssetDirectory), 0, 0};
        directory.dataOffset = Align(directory.entriesOffset + assets.size() * sizeof(R::AssetEntry));
        result.resize(directory.dataOffset);
        Put(std::span<Byte>(result), directoryOffset, directory);
        for (size_t i = 0; i < assets.size(); ++i)
        {
            auto asset = assets[i];
            asset.offset = Align(result.size());
            result.resize(asset.offset);
            result.insert(result.end(), payload.begin() + assets[i].offset,
                          payload.begin() + assets[i].offset + asset.size);
            Put(std::span<Byte>(result), directory.entriesOffset + i * sizeof(asset), asset);
        }
    }
    header.size = result.size();
    Put(std::span<Byte>(result), 0, header);
    // v4 native-import data is keyed by captured rows; RetailEditorData origins map edited rows back to them.
    R::AppendExtension(result, R::ExtensionBytes(payload));
    Publish(result, metadata);
}
void EditorT8Retail::EditRows(size_t table, uint32_t first, const std::vector<int> &ids, const std::set<int> &deleted)
{
    Mutate([&] {
        RecordEdit edit(first, ids, deleted, Count(table));
        std::vector<Byte> replacement;
        std::vector<uint32_t> origins;
        for (auto source : edit.sources)
        {
            if (source >= 0)
            {
                auto record = Record(table, source);
                replacement.insert(replacement.end(), record.begin(), record.end());
                origins.push_back(m_metadata->origins[table][source]);
            }
            else
            {
                replacement.resize(replacement.size() + R::Tables[table].stride);
                origins.push_back(UINT32_MAX);
            }
        }
        Rebuild(table, edit, replacement, origins);
        // Zero-filled new rows (not reused deleted bytes) are not list terminators for the SaveRecord that follows.
        for (size_t i = 0; i < edit.sources.size(); ++i)
            if (edit.sources[i] < 0)
                m_insertedRows.insert(std::pair<size_t, uint32_t>(table, uint32_t(first + i)));
    });
}
size_t EditorT8Retail::ListSize(size_t table, uint32_t first) const
{
    Record(table, first);
    if (table == 5 || table == 15)
    {
        size_t length = 1;
        const auto source = table == 5 ? 4 : 14;
        for (uint32_t row = 0; row < Count(source); ++row)
        {
            auto record = Record(source, row);
            if (At<uint64_t>(record, 8) == m_entries[table].address + first * m_entries[table].stride)
            {
                auto count = table == 5 ? uint64_t(At<uint32_t>(record, 4)) : At<uint16_t>(record, 2);
                if (count > Count(table) - first)
                    throw std::out_of_range("Referenced list exceeds table");
                length = (std::max)(length, size_t(count));
            }
        }
        return length;
    }
    if (table != 1 && table != 2 && table != 6 && table != 7 && table != 9 && table != 10 && table != 11 && table != 13)
        return 1;
    for (uint32_t row = first; row < Count(table); ++row)
    {
        auto bytes = Record(table, row);
        bool end = false;
        if (table == 1)
            end = At<uint32_t>(bytes, 0) == 1100;
        if (table == 6 || table == 7)
            end = At<uint64_t>(bytes, 0) == 0x8000 || (table == 7 && At<uint64_t>(bytes, 0) == RetailT8Commands::GroupEnd);
        if (table == 9) // property id 0 ends the list, whatever the frame (as in the converters)
            end = At<uint32_t>(bytes, offsetof(StructsT8::ExtraMoveProperty, id)) == 0;
        if (table == 10 || table == 11)
            end = At<uint32_t>(bytes, 8) == 1100;
        if (table == 13)
            end = At<uint32_t>(bytes, 0) == UINT32_MAX;
        if (table == 2)
        {
            auto pointer = At<uint64_t>(bytes, 0);
            const auto &req = m_entries[1];
            if (pointer >= req.address && pointer - req.address < req.count * req.stride &&
                (pointer - req.address) % req.stride == 0)
                end = At<uint32_t>(Record(1, uint32_t((pointer - req.address) / req.stride)), 0) == 1100;
        }
        if (end)
            return row - first + 1;
    }
    throw std::runtime_error("List has no bounded terminator");
}
std::vector<InputMap> EditorT8Retail::GetFormFieldsList(EditorWindowType type, uint16_t id,
                                                        VectorSet<std::string> &order)
{
    return GetFormFieldsList(type, id, order, int(ListSize(TableIndex(type), id)));
}
std::vector<InputMap> EditorT8Retail::GetFormFieldsList(EditorWindowType type, uint16_t id,
                                                        VectorSet<std::string> &order, int count)
{
    return GetFormFieldsListWide(type, id, order, count);
}
std::vector<InputMap> EditorT8Retail::GetFormFieldsListWide(EditorWindowType type, uint32_t id,
                                                            VectorSet<std::string> &order, int count)
{
    auto table = TableIndex(type);
    if (!count)
        count = int(ListSize(table, id));
    if (count < 0 || uint64_t(id) + count > Count(table))
        throw std::out_of_range("List extent");
    std::vector<InputMap> result;
    try
    {
        for (int i = 0; i < count; ++i)
            result.push_back(RecordFields(table, id + i, order));
    }
    catch (...)
    {
        for (auto &fields : result)
            for (auto &[name, field] : fields)
                delete field;
        throw;
    }
    return result;
}
void EditorT8Retail::ModifyListSize(EditorWindowType type, unsigned int first, const std::vector<int> &ids,
                                    const std::set<int> &deleted)
{
    EditRows(TableIndex(type), first, ids, deleted);
}
uint32_t EditorT8Retail::DuplicateStructure(EditorWindowType type, uint32_t first, size_t count)
{
    auto table = TableIndex(type);
    if (!count)
        count = ListSize(table, first);
    if (first >= Count(table) || count > Count(table) - first)
        throw std::out_of_range("Duplicate extent");
    return Mutate([&] {
        auto destination = uint32_t(Count(table));
        std::vector<int> ids(count, -1);
        RecordEdit edit(destination, ids, {}, Count(table));
        std::vector<Byte> replacement;
        std::vector<uint32_t> origins;
        for (size_t i = 0; i < count; ++i)
        {
            auto record = Record(table, first + i);
            replacement.insert(replacement.end(), record.begin(), record.end());
            origins.push_back(m_metadata->origins[table][first + i]);
        }
        Rebuild(table, edit, replacement, origins);
        // A copied move keeps its source row's check slots; rewrite them for the new row (native layout: filler
        // 0x765 + row, plain copy in slot row % 8 - 4 when row % 8 >= 4; see SetPlainCopy).
        if (table == 12)
            for (size_t i = 0; i < count; ++i)
            {
                const auto row = uint32_t(destination + i);
                auto* move = m_movesetData + m_entries[12].offset + size_t(row) * m_entries[12].stride;
                for (uint32_t offset : {0x00u, 0x20u, 0x58u, 0x78u, 0xd0u, 0xf0u})
                {
                    uint64_t value, key;
                    memcpy(&value, move + offset, 8); memcpy(&key, move + offset + 8, 8);
                    const auto plain = Decode(value, key);
                    if (!plain) continue;
                    const uint32_t filler = 0x765 + row;
                    for (int slot = 0; slot < 4; ++slot) memcpy(move + offset + 0x10 + slot * 4, &filler, 4);
                    SetPlainCopy(move + offset, row, *plain);
                }
            }
        return destination;
    });
}
bool EditorT8Retail::CanDeleteRecord(size_t table, uint32_t row) const
{
    if (row >= Count(table))
        return false;
    if (table != 12)
        return true;
    // Stock IDs also occur in native property handlers. Keep their indices stable,
    // as in the T7 UI; appended custom moves can be removed safely.
    for (auto id : m_metadata->remaps[12])
        if (id != UINT32_MAX && id >= row)
            return false;
    return true;
}
void EditorT8Retail::DeleteStructures(EditorWindowType type, uint32_t first, size_t count)
{
    auto table = TableIndex(type);
    if (!count)
        count = 1;
    if (first >= Count(table) || count > Count(table) - first)
        throw std::out_of_range("Delete extent");
    if (!CanDeleteRecord(table, first))
        throw std::invalid_argument("Native move IDs must remain stable; only appended custom moves can be deleted");
    std::set<int> deleted;
    for (size_t i = 0; i < count; ++i)
        deleted.insert(int(first + i));
    EditRows(table, first, {}, deleted);
}
uint32_t EditorT8Retail::CreateNew(EditorWindowType type)
{
    auto table = TableIndex(type);
    return MutateBatch([&] {
        // Address of a requirements row holding the 1100 terminator (an empty condition list), created if missing.
        auto emptyRequirements = [&] {
            uint32_t terminal = UINT32_MAX;
            for (uint32_t i = 0; i < Count(1); ++i)
                if (At<uint32_t>(Record(1, i), 0) == 1100)
                {
                    terminal = i;
                    break;
                }
            if (terminal == UINT32_MAX)
                terminal = CreateNew(Type(1)) + 1;
            return m_entries[1].address + terminal * m_entries[1].stride;
        };
        if (table == 12)
        {
            // A new move starts as a copy of the idle stance (valid animation, hurtbox and hit level), with its
            // own one-row cancel list (AUTO back to idle), no extra cancel lists, the idle transition and no
            // voiceclips or properties. Its hit conditions are the idle's (no damage). T7's CreateNewMove
            // copied move 0 and shared every list; the competitor's "Create Empty Move" builds the cancel list.
            const auto idle = ResolveMove(0x8001);
            auto id = DuplicateStructure(type, idle, 1);
            const auto requirement = emptyRequirements();
            if (!Count(8))
                CreateNew(Type(8));
            const auto cancel = uint32_t(Count(6));
            {
                RecordEdit edit(cancel, std::vector<int>(1, -1), {}, Count(6));
                std::vector<Byte> rows(R::Tables[6].stride);
                Put(std::span<Byte>(rows), 0, RetailT8Commands::ListEnd);
                Put(std::span<Byte>(rows), 8, requirement);
                Put(std::span<Byte>(rows), 16, m_entries[8].address);
                Put(std::span<Byte>(rows), 36, uint16_t(0x8001));
                Rebuild(6, edit, rows, std::vector<uint32_t>(1, UINT32_MAX));
            }
            // Rebuild renumbers the file-local table addresses: read them after it.
            auto move = std::span<Byte>(m_movesetData + m_entries[12].offset + id * R::Tables[12].stride, R::Tables[12].stride);
            Put(move, 0x98, m_entries[6].address + cancel * m_entries[6].stride);
            for (uint32_t offset : {0xa0u, 0xb0u, 0xc0u})
            {
                Put(move, offset, uint64_t(0));
                Put(move, offset + 8, int32_t(0));
            }
            Put(move, 0xcc, uint16_t(0x8001));
            for (uint32_t offset : {0x130u, 0x138u, 0x140u, 0x148u})
                Put(move, offset, uint64_t(0));
            RenameMove(id, "tkm_" + std::to_string(id));
            return id;
        }
        uint64_t requirement = 0;
        if (table == 2 || table == 6 || table == 7 || table == 9 || table == 10 || table == 11)
            requirement = emptyRequirements();
        if ((table == 6 || table == 7) && !Count(8))
            CreateNew(Type(8));
        if (table == 2 && !Count(0))
            CreateNew(Type(0));
        auto first = uint32_t(Count(table));
        // A new extra property list is its terminator only ({frame 0, id 0}), like start/end property lists: property id
        // 0 ends the list, so a blank first row would end it too. Rows are added with the list form's insert.
        size_t count = (table == 1 || table == 6 || table == 7) ? 2 : 1;
        RecordEdit edit(first, std::vector<int>(count, -1), {}, Count(table));
        std::vector<Byte> rows(count * R::Tables[table].stride);
        if (table == 1)
            Put(std::span<Byte>(rows), R::Tables[table].stride, uint32_t(1100));
        if (table == 6 || table == 7)
        {
            Put(std::span<Byte>(rows), R::Tables[table].stride, uint64_t(table == 7 ? RetailT8Commands::GroupEnd : RetailT8Commands::ListEnd));
            for (size_t row = 0; row < count; ++row)
            {
                Put(std::span<Byte>(rows), row * 40 + 8, requirement);
                Put(std::span<Byte>(rows), row * 40 + 16, m_entries[8].address);
            }
        }
        if (table == 9)
            Put(std::span<Byte>(rows), 8, requirement);
        if (table == 10 || table == 11)
            Put(std::span<Byte>(rows), 0, requirement);
        if (table == 10 || table == 11)
            Put(std::span<Byte>(rows), 8, uint32_t(1100));
        if (table == 13)
            std::fill(rows.begin(), rows.end(), 0xff);
        if (table == 2)
        {
            Put(std::span<Byte>(rows), 0, requirement);
            Put(std::span<Byte>(rows), 16, m_entries[0].address);
        }
        Rebuild(table, edit, rows, std::vector<uint32_t>(count, UINT32_MAX));
        return first;
    });
}
void EditorT8Retail::RenameMove(uint32_t row, std::string_view name)
{
    if (name.empty() || name.size() > 255 || name.find('\0') != name.npos)
        throw std::invalid_argument("Move names require 1 to 255 bytes");
    Mutate([&] {
        auto hash = RetailNames::Hash(name);
        auto metadata = *m_metadata;
        if (auto found = metadata.names.find(hash); found != metadata.names.end() && found->second != name)
            throw std::invalid_argument("Name hash collision");
        metadata.names[hash] = name;
        std::vector<Byte> bytes(m_movesetData, m_movesetData + m_movesetDataSize);
        auto record = Record(12, row);
        Put(std::span<Byte>(bytes), m_entries[12].offset + row * 0x448, Encode(hash, At<uint64_t>(record, 8)));
        // The name key (+0x00) is an encrypted field: its plain copy slot follows the new hash.
        SetPlainCopy(bytes.data() + m_entries[12].offset + row * 0x448, row, hash);
        Publish(bytes, metadata);
    });
}
void EditorT8Retail::SetAnimation(uint32_t row, uint32_t donor)
{
    Mutate([&] {
        auto source = Record(12, donor);
        Record(12, row);
        auto destination = m_movesetData + m_entries[12].offset + row * 0x448;
        // Animation key (+0x20): value and key from the donor. The four slots after them belong to this row (filler
        // 0x765 + row, plain copy at row % 8 - 4), so only the copy slot is rewritten, with the new hash.
        memcpy(destination + 0x20, source.data() + 0x20, 0x10);
        if (auto hash = Decode(At<uint64_t>(source, 0x20), At<uint64_t>(source, 0x28)))
            SetPlainCopy(destination + 0x20, row, *hash);
        memcpy(destination + 0x48, source.data() + 0x48, 0x10);
        memcpy(destination + 0x120, source.data() + 0x120, 4);
    });
}
uint32_t EditorT8Retail::FindAnimationFile(const wchar_t *path) const
{
    if (auto row = FindBoundAnimation(ReadPanmFile(std::filesystem::path(path))))
        return *row;
    throw std::runtime_error("This PANM is not embedded in the document; it is imported as a new animation on Apply.");
}
std::string EditorT8Retail::ImportAnimation(const wchar_t *path, int move)
{
    if (move < 0)
        throw std::out_of_range("Invalid destination move");
    const std::filesystem::path file(path);
    ImportPanm(uint32_t(move), ReadAnimationFile(file), Helpers::to_utf8(file.stem().wstring()));
    auto record = Record(12, uint32_t(move));
    return std::format("0x{:08X}", *Decode(At<uint64_t>(record, 0x20), At<uint64_t>(record, 0x28)));
}
