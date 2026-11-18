#include "Editor_t7_LiveRetail.hpp"
#include "Editor_t7.hpp"
#include "Importer_t8/Importer_t8.hpp"
#include "Importer_t8/RetailLiveEdit.hpp"
#include "Extractor_t8/RetailSnapshot.hpp"
#include "GameIDs.hpp"
#include "MovesetStructs.h"
#include <algorithm>
#include <optional>
#include <tuple>
#include <cstring>
#include <stdexcept>

namespace {
namespace S = StructsT7;
namespace G = StructsT7_gameAddr;
constexpr size_t NameBlock = 100, AnimationBlock = 101;
constexpr uint64_t Null = UINT64_MAX;
constexpr size_t Strides[19] = {
    sizeof(S::Reactions), sizeof(S::Requirement), sizeof(S::HitCondition), sizeof(S::Projectile),
    sizeof(S::Pushback), sizeof(S::PushbackExtradata), sizeof(S::Cancel), sizeof(S::Cancel),
    sizeof(S::CancelExtradata), sizeof(S::ExtraMoveProperty), sizeof(S::OtherMoveProperty),
    sizeof(S::OtherMoveProperty), sizeof(S::Move), sizeof(S::Voiceclip), sizeof(S::InputSequence),
    sizeof(S::Input), sizeof(S::UnknownParryRelated), sizeof(S::CameraData), sizeof(S::ThrowCamera)
};
// T7 reference fields: the index fields T7ImportUtils::ConvertMovesetIndexes turns into addresses at import.
struct Pointer { size_t table; uint32_t field; size_t target; };
#define T7_FIELD(type, member) uint32_t(offsetof(G::type, member))
const Pointer Pointers[] = {
    {0, T7_FIELD(Reactions, front_pushback), 4}, {0, T7_FIELD(Reactions, backturned_pushback), 4},
    {0, T7_FIELD(Reactions, left_side_pushback), 4}, {0, T7_FIELD(Reactions, right_side_pushback), 4},
    {0, T7_FIELD(Reactions, front_counterhit_pushback), 4}, {0, T7_FIELD(Reactions, downed_pushback), 4},
    {0, T7_FIELD(Reactions, block_pushback), 4},
    {2, T7_FIELD(HitCondition, requirements_addr), 1}, {2, T7_FIELD(HitCondition, reactions_addr), 0},
    {3, T7_FIELD(Projectile, hit_condition_addr), 2}, {3, T7_FIELD(Projectile, cancel_addr), 6},
    {4, T7_FIELD(Pushback, extradata_addr), 5},
    {6, T7_FIELD(Cancel, requirements_addr), 1}, {6, T7_FIELD(Cancel, extradata_addr), 8},
    {7, T7_FIELD(Cancel, requirements_addr), 1}, {7, T7_FIELD(Cancel, extradata_addr), 8},
    {10, T7_FIELD(OtherMoveProperty, requirements_addr), 1}, {11, T7_FIELD(OtherMoveProperty, requirements_addr), 1},
    {12, T7_FIELD(Move, name_addr), NameBlock}, {12, T7_FIELD(Move, anim_name_addr), NameBlock},
    {12, T7_FIELD(Move, anim_addr), AnimationBlock},
    {12, T7_FIELD(Move, cancel_addr), 6}, {12, T7_FIELD(Move, _0x28_cancel_addr), 6},
    {12, T7_FIELD(Move, _0x38_cancel_addr), 6}, {12, T7_FIELD(Move, _0x48_cancel_addr), 6},
    {12, T7_FIELD(Move, hit_condition_addr), 2}, {12, T7_FIELD(Move, voicelip_addr), 13},
    {12, T7_FIELD(Move, extra_move_property_addr), 9}, {12, T7_FIELD(Move, move_start_extraprop_addr), 10},
    {12, T7_FIELD(Move, move_end_extraprop_addr), 11},
    {14, T7_FIELD(InputSequence, input_addr), 15},
    {18, T7_FIELD(ThrowCamera, cameradata_addr), 17},
};
#undef T7_FIELD
}

T7LiveShadow::T7LiveShadow(const S::TKMovesetHeaderBlocks& blocks, std::vector<Byte> payload)
    : m_blocks(blocks), m_payload(std::move(payload)) {
    if (m_payload.size() < sizeof(G::MovesetInfo) || m_blocks.movesetBlock > m_payload.size())
        throw std::runtime_error("Invalid T7 moveset for live edition");
    const auto& info = *reinterpret_cast<const G::MovesetInfo*>(m_payload.data() + m_blocks.movesetInfoBlock);
    for (size_t t = 0; t < 19; ++t) {
        m_offset[t] = m_blocks.movesetBlock + info.table.entries[t].listAddr;
        m_count[t] = info.table.entries[t].listCount;
        if (m_count[t] && (m_offset[t] > m_payload.size() || m_count[t] > (m_payload.size() - m_offset[t]) / Strides[t]))
            throw std::runtime_error("Invalid T7 table for live edition");
    }
    status = GameProcessErrcode_PROC_ATTACHED;
}

std::span<const Byte> T7LiveShadow::Row(size_t table, uint64_t row) const {
    if (table >= 19 || row >= m_count[table]) throw std::out_of_range("T7 row outside its table");
    return std::span<const Byte>(m_payload).subspan(m_offset[table] + row * Strides[table], Strides[table]);
}

std::set<std::pair<size_t, uint64_t>> T7LiveShadow::TakeDirty() { return std::exchange(m_dirty, {}); }

bool T7LiveShadow::ReadMemoryRaw(gameAddr address, void* data, size_t size) const {
    if (address < Base || address - Base > m_payload.size() || size > m_payload.size() - (address - Base)) return false;
    memcpy(data, m_payload.data() + (address - Base), size);
    return true;
}

bool T7LiveShadow::WriteMemoryRaw(gameAddr address, const void* data, size_t size) {
    if (address < Base || address - Base > m_payload.size() || size > m_payload.size() - (address - Base)) return false;
    const uint64_t at = address - Base;
    memcpy(m_payload.data() + at, data, size);
    for (size_t t = 0; t < 19; ++t) {
        const auto begin = m_offset[t], end = m_offset[t] + m_count[t] * Strides[t];
        if (!m_count[t] || at + size <= begin || at >= end) continue;
        const auto first = (std::max)(at, begin), last = (std::min)(at + size, end);
        for (uint64_t row = (first - begin) / Strides[t]; row <= (last - 1 - begin) / Strides[t]; ++row) {
            m_dirty.insert({t, row});
            // The live writers store references as addresses: keep the row in file layout.
            const auto rowStart = begin + row * Strides[t];
            for (const auto& p : Pointers)
                if (p.table == t && at < rowStart + p.field + 8 && rowStart + p.field < at + size) StoreIndex(t, row, p.field);
        }
    }
    return true;
}

void T7LiveShadow::SyncFrom(std::span<const Byte> t7File) {
    if (t7File.size() < sizeof(TKMovesetHeader)) { m_invalid = "the moveset is missing"; return; }
    const auto& header = *reinterpret_cast<const TKMovesetHeader*>(t7File.data());
    if (header.block_list + sizeof(S::TKMovesetHeaderBlocks) > t7File.size() || header.moveset_data_start > t7File.size()) {
        m_invalid = "the moveset header is invalid"; return;
    }
    S::TKMovesetHeaderBlocks blocks; memcpy(&blocks, t7File.data() + header.block_list, sizeof(blocks));
    const auto payload = t7File.subspan(header.moveset_data_start);
    if (payload.size() < sizeof(G::MovesetInfo) || blocks.movesetBlock > payload.size()) { m_invalid = "the moveset is invalid"; return; }
    const auto& info = *reinterpret_cast<const G::MovesetInfo*>(payload.data() + blocks.movesetInfoBlock);
    for (size_t t = 0; t < 19; ++t)
        if (info.table.entries[t].listCount != m_count[t]) { m_invalid = "a list changed size"; return; }
    for (size_t t = 0; t < 19; ++t) {
        const auto offset = blocks.movesetBlock + info.table.entries[t].listAddr;
        if (m_count[t] && (offset > payload.size() || m_count[t] > (payload.size() - offset) / Strides[t])) { m_invalid = "a table is invalid"; return; }
        for (uint64_t row = 0; row < m_count[t]; ++row) {
            const auto* saved = payload.data() + offset + row * Strides[t];
            auto* copy = m_payload.data() + m_offset[t] + row * Strides[t];
            if (!memcmp(saved, copy, Strides[t])) continue;
            memcpy(copy, saved, Strides[t]);
            m_dirty.insert({t, row});
        }
    }
}

void T7LiveShadow::StoreIndex(size_t table, uint64_t row, uint32_t field) {
    const auto at = m_offset[table] + row * Strides[table] + field;
    uint64_t value; memcpy(&value, m_payload.data() + at, 8);
    const Pointer* pointer = nullptr;
    for (const auto& p : Pointers) if (p.table == table && p.field == field) pointer = &p;
    uint64_t stored = Null;
    if (value == 0) stored = Null;
    else if (pointer->target == NameBlock || pointer->target == AnimationBlock) {
        const auto base = Base + (pointer->target == NameBlock ? m_blocks.nameBlock : m_blocks.animationBlock);
        if (value < base) { m_invalid = "a move refers outside its name or animation block"; return; }
        stored = value - base;
    } else {
        const auto target = pointer->target, base = Base + m_offset[target];
        if (value < base || (value - base) % Strides[target] || (value - base) / Strides[target] >= m_count[target]) {
            m_invalid = "a reference points outside its T7 table"; return;
        }
        stored = (value - base) / Strides[target];
    }
    memcpy(m_payload.data() + at, &stored, 8);
}

T7RetailLive::T7RetailLive(std::span<const Byte> t7File)
    : m_conversion([&] {
        if (t7File.size() < sizeof(TKMovesetHeader)) throw std::runtime_error("Missing T7 moveset");
        const auto& header = *reinterpret_cast<const TKMovesetHeader*>(t7File.data());
        if (header.block_list_size < 8 || header.block_list + sizeof(S::TKMovesetHeaderBlocks) > t7File.size() || header.moveset_data_start > t7File.size())
            throw std::runtime_error("Invalid T7 moveset header");
        S::TKMovesetHeaderBlocks blocks; memcpy(&blocks, t7File.data() + header.block_list, sizeof(blocks));
        return T7ToRetailT8::LiveConversion(header, blocks, t7File.subspan(header.moveset_data_start), ImportProfile::Read(header));
    }()),
    m_shadow([&] {
        const auto& header = *reinterpret_cast<const TKMovesetHeader*>(t7File.data());
        S::TKMovesetHeaderBlocks blocks; memcpy(&blocks, t7File.data() + header.block_list, sizeof(blocks));
        return blocks;
    }(), std::vector<Byte>(t7File.begin() + reinterpret_cast<const TKMovesetHeader*>(t7File.data())->moveset_data_start, t7File.end())) {}

void T7RetailLive::Bind(gameAddr root, uint8_t slot, const GameInfo* game) {
    if (!game || game->gameId != GameId_T8 || game->minorVersion != GameVersions::T8::RETAIL) throw std::invalid_argument("Retail live edition needs retail T8");
    m_root = root; m_slot = slot; m_game = game;
    m_published = m_conversion.File();
    m_shadow.TakeDirty();
}

void T7RetailLive::Publish(GameProcess& process, GameData& game) {
    m_lastRecords = 0;
    if (!m_root) throw std::runtime_error("Live edition is not bound to an imported moveset; import again.");
    auto stop = [](const std::string& reason) {
        return std::runtime_error("Live edition stopped (" + reason + "): import the moveset again.");
    };
    if (!m_shadow.Invalid().empty()) throw stop(m_shadow.Invalid());
    const auto dirty = m_shadow.TakeDirty();
    if (dirty.empty()) return;
    for (const auto& [table, row] : dirty)
        if (auto reason = m_conversion.UpdateRow(table, row, m_shadow.Row(table, row))) throw stop(*reason);
    ImporterT8 importer(process, game, m_game);
    const auto& current = m_conversion.File();
    const auto result = RetailLiveEdit::Sync(process, importer.GetCharacterAddress(m_slot), m_root, m_published, current, importer.Offsets());
    if (result.structural) throw stop(result.reason);
    m_lastRecords = result.records;
    // Keep the published copy current. Edits change table rows only, never the asset data after the tables.
    if (m_published.size() != current.size()) throw stop("the converted moveset changed size");
    const auto& outer = *reinterpret_cast<const TKMovesetHeader*>(current.data());
    const auto payload = std::span<const Byte>(current).subspan(outer.moveset_data_start);
    uint64_t end = 0;
    for (size_t t = 0; t < 20; ++t) {
        const auto rows = RetailT8Snapshot::Table(payload, t);
        if (!rows.empty()) end = (std::max)(end, static_cast<uint64_t>(rows.data() - payload.data()) + rows.size());
    }
    memcpy(m_published.data() + outer.moveset_data_start, payload.data(), static_cast<size_t>(end));
}

// -- EditorT7 -- //

void EditorT7::SetRetailLive(std::unique_ptr<T7RetailLive> live) { m_retailLive = std::move(live); }

namespace {
// The table rows of `applied` that differ from `baseline` (two T7 moveset files). Nothing when anything else differs
// (layout, table sizes, names, animations, movelist, aliases): the game's copy of that data is not row-addressable.
std::optional<std::vector<std::pair<size_t, uint64_t>>> ChangedT7Rows(std::span<const Byte> baseline, std::span<const Byte> applied)
{
    if (baseline.size() != applied.size() || baseline.size() < sizeof(TKMovesetHeader)) return std::nullopt;
    const auto& header = *reinterpret_cast<const TKMovesetHeader*>(applied.data());
    const auto& before = *reinterpret_cast<const TKMovesetHeader*>(baseline.data());
    if (header.moveset_data_start != before.moveset_data_start || header.block_list != before.block_list ||
        header.block_list + sizeof(S::TKMovesetHeaderBlocks) > applied.size() || header.moveset_data_start > applied.size()) return std::nullopt;
    S::TKMovesetHeaderBlocks blocks; memcpy(&blocks, applied.data() + header.block_list, sizeof(blocks));
    if (memcmp(&blocks, baseline.data() + header.block_list, sizeof(blocks))) return std::nullopt;
    const auto start = static_cast<size_t>(header.moveset_data_start), size = applied.size() - start;
    if (size < sizeof(G::MovesetInfo) || blocks.movesetBlock > size) return std::nullopt;
    const auto& info = *reinterpret_cast<const G::MovesetInfo*>(applied.data() + start + blocks.movesetInfoBlock);
    // Table regions, in payload order
    std::vector<std::tuple<uint64_t, uint64_t, size_t>> regions; // begin, end, table
    for (size_t t = 0; t < 19; ++t) {
        const auto begin = blocks.movesetBlock + info.table.entries[t].listAddr, count = info.table.entries[t].listCount;
        if (!count) continue;
        if (begin > size || count > (size - begin) / Strides[t]) return std::nullopt;
        regions.emplace_back(begin, begin + count * Strides[t], t);
    }
    std::sort(regions.begin(), regions.end());
    std::vector<std::pair<size_t, uint64_t>> rows;
    uint64_t at = 0;
    for (const auto& [begin, end, t] : regions) {
        if (begin < at) return std::nullopt; // overlapping tables
        if (memcmp(applied.data() + start + at, baseline.data() + start + at, begin - at)) return std::nullopt;
        for (uint64_t row = 0; row < (end - begin) / Strides[t]; ++row) {
            const auto offset = start + begin + row * Strides[t];
            if (memcmp(applied.data() + offset, baseline.data() + offset, Strides[t])) rows.push_back({t, row});
        }
        at = end;
    }
    if (memcmp(applied.data() + start + at, baseline.data() + start + at, size - at)) return std::nullopt;
    // The file header (crc, dates) is not in the game; only the payload is compared.
    return rows;
}
}

bool EditorT7::Live_OnApplied(gameAddr live)
{
	const std::span<const Byte> applied{m_moveset, static_cast<size_t>(m_movesetSize)};
	if (m_retailLive) {
		if (!m_retailLive->Bound()) return false;
		// The applied rows reach the game as live field edits do; an edit one row cannot carry stops with the reason.
		m_retailLive->Shadow().SyncFrom(applied);
		m_retailLive->Publish(m_process, m_game);
		return true;
	}
	// T7: the changed rows are written into the moveset the game holds, relocated as the importer relocates them
	// (T7ImportUtils::ConvertMovesetIndexes). Anything else changed (names, animations, a list's size) needs an import.
	if (m_liveBaseline.empty() || !live) return false;
	const auto rows = ChangedT7Rows(m_liveBaseline, applied);
	if (!rows) return false;
	if (!rows->empty()) {
		const auto& info = *reinterpret_cast<const G::MovesetInfo*>(m_movesetData + m_offsets->movesetInfoBlock);
		const auto blockStart = live + m_offsets->movesetBlock;
		GameProcess::WriteBatch transaction(m_process);
		for (const auto& [table, row] : *rows) {
			std::vector<Byte> bytes(Strides[table]);
			const auto fileOffset = m_offsets->movesetBlock + info.table.entries[table].listAddr + row * Strides[table];
			memcpy(bytes.data(), m_movesetData + fileOffset, bytes.size());
			for (const auto& p : Pointers) {
				if (p.table != table) continue;
				uint64_t value; memcpy(&value, bytes.data() + p.field, 8);
				if (p.target == NameBlock) value += live + m_offsets->nameBlock;
				else if (p.target == AnimationBlock) value += live + m_offsets->animationBlock;
				else if (value == Null) value = 0;
				else value = blockStart + info.table.entries[p.target].listAddr + value * Strides[p.target];
				memcpy(bytes.data() + p.field, &value, 8);
			}
			m_process.writeBytes(blockStart + info.table.entries[table].listAddr + row * Strides[table], bytes.data(), bytes.size());
		}
		const auto result = transaction.Commit();
		if (!result.ok) throw std::runtime_error(result.rollbackComplete ? "Live edit failed; the previous values were restored." :
			"Live edit failed and rollback was incomplete. Re-import before editing live.");
	}
	m_liveBaseline.assign(applied.begin(), applied.end());
	return true;
}

void EditorT7::Live_OnRetailFieldEdit(EditorWindowType type, int id, EditorInput* field)
{
    auto& shadow = m_retailLive->Shadow();
    struct Redirect {
        EditorT7& editor; gameAddr root;
        ~Redirect() { editor.m_liveTarget = nullptr; editor.live_loadedMoveset = root; }
    } redirect{*this, live_loadedMoveset};
    m_liveTarget = &shadow;
    live_loadedMoveset = shadow.Address();
    Live_Dispatch(type, id, field);
    m_liveTarget = nullptr;
    live_loadedMoveset = redirect.root;
    m_retailLive->Publish(m_process, m_game);
}
