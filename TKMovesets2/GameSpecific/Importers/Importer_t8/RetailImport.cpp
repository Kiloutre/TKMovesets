#include "ConversionRules.hpp"
#include <optional>
#include <format>
#include "RetailImport.hpp"
#include "RetailCommands.hpp"
#include "GameIDs.hpp"
#include "Extractor_t8/RetailRuntime.hpp"
#include "RetailResourceLoader.hpp"
#include <cstring>
#include <stdexcept>
#include <bit>
#include <map>
#include <set>

namespace RetailT8Import {
namespace {
namespace R = RetailT8Snapshot;
template<class T> T Read(std::span<const Byte> bytes, size_t offset) {
    if (offset > bytes.size() || sizeof(T) > bytes.size() - offset)
        throw std::runtime_error("Truncated retail import field");
    T value; memcpy(&value, bytes.data() + offset, sizeof(value)); return value;
}
void Write(std::vector<Byte>& bytes, size_t offset, uint64_t value) {
    memcpy(bytes.data() + offset, &value, sizeof(value));
}
size_t Align(size_t n) { return (n + 7) & ~size_t(7); }
uint32_t Name(std::span<const Byte> bytes, size_t offset) {
    const auto encrypted = Read<uint64_t>(bytes, offset), key = Read<uint64_t>(bytes, offset + 8);
    uint32_t checksum = 0, value = static_cast<uint32_t>(encrypted);
    for (int shift = 0; shift < 32; shift += 8) { checksum ^= value ^ static_cast<uint32_t>(std::rotl(key, shift + 8)); value >>= 8; }
    if ((encrypted >> 32) != (checksum ? checksum : 1)) throw std::runtime_error("Invalid encrypted retail name checksum");
    auto scrambled = encrypted ^ 0x1d;
    return static_cast<uint32_t>((std::rotl(key, static_cast<int>(scrambled & 31)) & ~uint64_t(31)) ^ scrambled);
}
// Typed runtime pointer fields. No pointer-shaped integer scanning: encrypted
// values, animation handles, commands, aliases and parameters must stay intact.
}
std::vector<size_t> BindAnimations(std::span<const Byte> captured, std::span<const Byte> loaded, bool convertedT7,
    const RetailEditorData::Metadata* metadata, size_t fallbackRow, BindStats* stats) {
    if (captured.size() % 0x448 || loaded.size() % 0x448 || loaded.empty())
        throw std::runtime_error("Retail move records are truncated");
    const auto loadedRows = loaded.size() / 0x448;
    if (fallbackRow >= loadedRows) fallbackRow = 0;
    BindStats local;
    // Loaded rows whose identity differs from the export's are only counted (to prefer the closest loaded moveset).
    if (metadata && !convertedT7) {
        for (size_t row = 0; row < metadata->moveNames.size(); ++row)
            if (row >= loadedRows || Name(loaded, row * 0x448) != metadata->moveNames[row]) ++local.identityMismatches;
        if (loadedRows > metadata->moveNames.size()) local.identityMismatches += loadedRows - metadata->moveNames.size();
    }
    std::map<uint32_t, size_t> animations;
    for (size_t row = 0; row < loadedRows; ++row)
        animations.try_emplace(Name(loaded, row * 0x448 + 0x20), row);
    std::vector<size_t> result;
    for (size_t row = 0; row < captured.size() / 0x448; ++row) {
        if (!convertedT7 && !metadata && (row >= loadedRows || Name(captured, row * 0x448) != Name(loaded, row * 0x448)))
            ++local.identityMismatches;
        auto hash = Name(captured, row * 0x448 + 0x20);
        auto match = animations.find(hash);
        const auto origin = metadata ? metadata->origins[12].at(row) : row;
        if (origin < loadedRows && Name(loaded, origin * 0x448 + 0x20) == hash) result.push_back(origin);
        else if (match != animations.end()) result.push_back(match->second);
        else { result.push_back(fallbackRow); ++local.missingAnimations; } // the loaded moveset does not play it: idle
    }
    if (stats) *stats = local;
    return result;
}
std::optional<std::vector<Byte>> RebaseExternalCancel(std::vector<Byte> entry,
    const std::function<std::pair<uint64_t, uint64_t>(size_t table)>& source,
    std::span<const R::Entry> entries, const RetailEditorData::Metadata* metadata,
    const std::function<uint64_t(size_t table, uint64_t id)>& place) {
    if (entry.size() != 40) throw std::runtime_error("Truncated external retail cancel");
    const bool terminator = Read<uint64_t>(entry, 0) == 0x8000;
    for (auto [member, table] : {std::pair<size_t, size_t>{8, 1}, {16, 8}}) {
        const auto pointer = Read<uint64_t>(entry, member);
        if (!pointer) continue;
        const auto [base, count] = source(table);
        const auto stride = entries[table].stride;
        if (pointer < base || pointer - base >= count * stride || (pointer - base) % stride)
            throw std::runtime_error("External cancel points outside the source moveset's tables");
        auto id = (pointer - base) / stride;
        if (metadata) id = id < metadata->remaps[table].size() ? metadata->remaps[table][id] : UINT64_MAX;
        if (id >= entries[table].count) { // deleted by the edit
            if (!terminator) return std::nullopt;
            Write(entry, member, uint64_t(0)); continue;
        }
        Write(entry, member, place(table, id));
    }
    if (metadata) {
        const auto command = Read<uint64_t>(entry, 0);
        const size_t target = command == RetailT8Commands::GroupCancel ? 7 : 12;
        const auto id = Read<uint16_t>(entry, 36);
        if (id < metadata->remaps[target].size()) {
            const auto mapped = metadata->remaps[target][id];
            if (mapped <= UINT16_MAX) { const auto value = uint16_t(mapped); memcpy(entry.data() + 36, &value, 2); }
            else if (!terminator) return std::nullopt;
        }
        if (RetailT8Commands::IsInputSequence(command) && RetailT8Commands::InputSequenceId(command) < metadata->remaps[14].size()) {
            const auto mapped = metadata->remaps[14][RetailT8Commands::InputSequenceId(command)];
            if (mapped < 0x10000 - RetailT8Commands::InputSequenceBase)
                Write(entry, 0, (command & 0xffffffff00000000ULL) | (RetailT8Commands::InputSequenceBase + mapped));
            else if (!terminator) return std::nullopt;
        }
    }
    return entry;
}
std::vector<Byte> TerminatorCancel(std::span<const Byte> payload, std::span<const R::Entry> entries,
    const std::function<uint64_t(size_t table, uint64_t id)>& place) {
    const auto cancels = R::Table(payload, 6);
    auto fileTable = [&](size_t table) { return std::pair<uint64_t, uint64_t>{entries[table].address, entries[table].count}; };
    for (size_t at = 0; at + 40 <= cancels.size(); at += 40) {
        if (Read<uint64_t>(cancels, at) != 0x8000) continue;
        try {
            if (auto rebased = RebaseExternalCancel(std::vector<Byte>(cancels.begin() + at, cancels.begin() + at + 40),
                fileTable, entries, nullptr, place)) return *rebased;
        } catch (const std::exception&) {}
    }
    std::vector<Byte> bare(40, 0);
    Write(bare, 0, uint64_t(0x8000));
    return bare;
}
gameAddr ReadyAnimation(const GameProcess& process, gameAddr container, uint32_t domain, uint32_t id, gameAddr compiledVtable) {
    namespace RT = RetailT8Runtime;
    if (!compiledVtable) throw std::invalid_argument("Missing the compiled animation vtable");
    for (const uint32_t key : {uint32_t(0x81), domain}) {
        try {
            const auto map = RT::DomainMap(process, container, key);
            const auto node = map ? RT::MapNode(process, map, id) : 0;
            const auto entry = node ? RT::Read<uint64_t>(process, node + 0x18) : 0;
            const auto resource = entry ? RT::Read<uint64_t>(process, entry + 8) : 0;
            if (!resource) continue;
            const auto compiled = RT::Read<uint64_t>(process, resource + 0x190), request = RT::Read<uint64_t>(process, resource + 0x140);
            if (compiled && RT::Read<uint64_t>(process, resource + 0x198) && request && RT::Read<uint32_t>(process, request + 0x30) == 2 &&
                RT::Read<uint64_t>(process, compiled) == compiledVtable) return resource;
        } catch (const std::exception&) {}
    }
    return 0;
}

uint64_t FallBackMissingCarriers(std::vector<size_t>& rows, std::span<const Byte> payload, const std::set<uint32_t>& missing) {
    if (missing.empty()) return 0;
    const auto moves = R::Table(payload, 12);
    if (rows.size() != moves.size() / 0x448) throw std::runtime_error("Carrier fallback dimensions differ");
    const auto idle = Read<uint16_t>(payload, Read<R::Header>(payload, 0).sourceHeaderOffset + 0xaa);
    if (idle >= rows.size()) throw std::runtime_error("Retail idle alias is out of range");
    const auto target = rows[idle];
    uint64_t count = 0;
    for (size_t row = 0; row < rows.size(); ++row) {
        const auto handle = Read<uint64_t>(moves, row * 0x448 + 0x50);
        if ((handle >> 32) && missing.count(static_cast<uint32_t>(handle))) { rows[row] = target; ++count; }
    }
    return count;
}
Plan Prepare(const TKMovesetHeader& outer, std::span<const Byte> payload, const ExternalCancels& external) {
    if (outer.gameId != GameId_T8 || outer.minorVersion != GameVersions::T8::RETAIL)
        throw std::runtime_error("Retail import requires a retail T8 snapshot");
    R::Validate(payload);
    const auto header = Read<R::Header>(payload, 0);
    if (outer.game_specific_flags != (header.version == 1 ? R::TablesOnlyFlag : R::RuntimeAssetsFlag))
        throw std::runtime_error("Retail import snapshot version/flags differ");
    Plan plan;
    std::array<R::Entry, R::Tables.size()> entries;
    plan.tables.assign(payload.begin() + header.sourceHeaderOffset,
        payload.begin() + header.sourceHeaderOffset + R::SourceHeaderSize);
    for (size_t i = 0; i < entries.size(); ++i) {
        auto& entry = entries[i] = Read<R::Entry>(payload, header.directoryOffset + i * sizeof(R::Entry));
        plan.offsets[i] = Align(plan.tables.size()); plan.counts[i] = entry.count;
        plan.tables.resize(plan.offsets[i]);
        plan.tables.insert(plan.tables.end(), payload.begin() + entry.offset,
            payload.begin() + entry.offset + entry.count * entry.stride);
        Write(plan.tables, R::Tables[i].pointerOffset, 0);
        if (entry.count) plan.relocations.push_back({R::Tables[i].pointerOffset, plan.offsets[i]});
    }
    auto dependency = [&](uint64_t field, const char* kind) {
        const auto value = Read<uint64_t>(plan.tables, field);
        if (value) plan.dependencies.push_back({field, value, kind});
        Write(plan.tables, field, 0);
    };
    for (size_t field = 0x10; field < 0x30; field += 8) dependency(field, "header_string");
    dependency(0x170, "header_string_block_end");
    for (size_t field = 0x2b0; field <= 0x310; field += 8) dependency(field, "header_runtime_resource");
    auto issue = [&](size_t table, uint64_t record, uint32_t field, uint64_t source, size_t target, const char* reason) {
        ++plan.issueCount;
        if (plan.issues.size() < 256) plan.issues.push_back({table, record, field, source, target, reason});
    };
    for (const auto& field : PointerFields) {
        const auto& source = entries[field.table]; const auto& target = entries[field.target];
        for (uint64_t row = 0; row < source.count; ++row) {
            const auto offset = plan.offsets[field.table] + row * source.stride + field.offset;
            const auto value = Read<uint64_t>(plan.tables, offset);
            Write(plan.tables, offset, 0);
            // Optional cancel banks contain inactive data when their signed gate
            // is <=0. Do not reinterpret that data as a live pointer.
            if (field.table == 12 && (field.offset == 0xa0 || field.offset == 0xb0 || field.offset == 0xc0) &&
                Read<int32_t>(plan.tables, offset + 8) <= 0) continue;
            if (!value) {
                if (field.table == 14 && Read<uint16_t>(plan.tables, offset - 6))
                    issue(field.table, row, field.offset, value, field.target, "Nonempty input sequence has no input pointer");
                continue; // Runtime NULL; index zero is represented by target.address.
            }
            if (!target.count || value < target.address || value - target.address >= target.count * target.stride ||
                (value - target.address) % target.stride) {
                if (external && field.table == 12 && field.target == 6) {
                    auto list = external(row, field.offset);
                    if (list.empty() || list.size() % 40 || list.size() > 4096 * 40 ||
                        Read<uint64_t>(list, list.size() - 40) != 0x8000 || plan.tables.size() + list.size() > (272ULL << 20))
                        throw std::runtime_error("Invalid external retail cancel list");
                    auto start = Align(plan.tables.size()); plan.tables.resize(start);
                    plan.tables.insert(plan.tables.end(), list.begin(), list.end());
                    plan.relocations.push_back({offset, start});
                    for (size_t entry = 0; entry < list.size(); entry += 40) {
                        for (auto [member, table] : {std::pair<size_t, size_t>{8, 1}, {16, 8}}) {
                            auto pointer = Read<uint64_t>(list, entry + member);
                            Write(plan.tables, start + entry + member, 0);
                            const auto& destination = entries[table];
                            if (!pointer) continue;
                            if (pointer < destination.address || pointer - destination.address >= destination.count * destination.stride ||
                                (pointer - destination.address) % destination.stride)
                                throw std::runtime_error("External cancel dependency is outside imported table");
                            plan.relocations.push_back({start + entry + member, plan.offsets[table] + pointer - destination.address});
                        }
                    }
                    continue;
                }
                issue(field.table, row, field.offset, value, field.target, "Pointer is outside its typed target table or misaligned");
                continue;
            }
            const auto index = (value - target.address) / target.stride;
            if (field.table == 14 && Read<uint16_t>(plan.tables, offset - 6) > target.count - index) {
                issue(field.table, row, field.offset, value, field.target, "Input sequence extends beyond input table");
                continue;
            }
            plan.relocations.push_back({offset, plan.offsets[field.target] + index * target.stride});
        }
    }
    for (uint64_t row = 0; row < entries[12].count; ++row) {
        const auto offset = plan.offsets[12] + row * entries[12].stride;
        dependency(offset + 0x40, "move_name_string");
        dependency(offset + 0x48, "move_animation_string");
        const auto handle = Read<uint64_t>(plan.tables, offset + 0x50);
        if (handle) plan.dependencies.push_back({offset + 0x50, handle, "body_animation_handle"});
    }
    for (const auto& asset : R::Assets(payload)) {
        if (asset.size) ++plan.availableAnimations;
        else ++plan.unavailableAnimations;
    }
    // A failed graph must not expose a partially relocated buffer.
    if (!plan.Ok()) { plan.tables.clear(); plan.relocations.clear(); }
    return plan;
}
std::vector<Byte> Plan::RelocateTables(gameAddr base) const {
    if (!Ok() || tables.empty()) throw std::runtime_error("Cannot relocate an invalid retail import plan");
    if (!base || base % 8 || tables.size() > UINT64_MAX - base)
        throw std::runtime_error("Invalid retail import allocation address");
    auto bytes = tables;
    for (const auto& fix : relocations) {
        if (fix.field > bytes.size() || sizeof(uint64_t) > bytes.size() - fix.field || fix.target >= bytes.size())
            throw std::runtime_error("Invalid retail import relocation");
        Write(bytes, fix.field, base + fix.target);
    }
    return bytes;
}

void ApplyConvertedIdentity(std::span<Byte> header, std::span<Byte> moves, uint32_t target) {
    if (header.size() < R::SourceHeaderSize || moves.size() % 0x448) throw std::runtime_error("Invalid retail identity records");
    auto code = [](uint32_t n) { return (n << 16) | ((0x10000U - n) & 0xffffU); };
    const uint32_t self = code(target - 1), next = code(target);
    memcpy(header.data() + 0x160, &self, 4); memcpy(header.data() + 0x164, &next, 4);
    for (size_t row = 0, at = 0xd0; at + 0x20 <= moves.size(); ++row, at += 0x448) {
        uint64_t key; memcpy(&key, moves.data() + at + 8, 8);
        // Same encoding as the game's encrypted move fields (see EditorT8Retail::Encode).
        const auto low = self ^ static_cast<uint32_t>(std::rotl(key, static_cast<int>(self & 31)) & ~31ULL) ^ 0x1dU;
        uint32_t check = 0, x = low;
        for (int shift = 0; shift < 32; shift += 8, x >>= 8) check ^= x ^ static_cast<uint32_t>(std::rotl(key, shift + 8));
        const uint64_t encoded = low | (uint64_t(check ? check : 1) << 32);
        memcpy(moves.data() + at, &encoded, 8);
        if (row % 8 >= 4) memcpy(moves.data() + at + 0x10 + (row % 8 - 4) * 4, &self, 4);
    }
}
uint32_t ProfileIdentity(const ImportProfile::Profile& profile, uint32_t own) {
    return profile.identity == ImportProfile::Profile::Identity::Fixed ? static_cast<uint32_t>(profile.character) : own;
}
uint64_t ApplyProfileConditions(std::span<Byte> records, const ImportProfile::Profile& profile) {
    if (records.size() % 20) throw std::runtime_error("Invalid retail requirement records");
    if (profile == ImportProfile::Profile{}) return 0;
    const auto& rules = ConversionRules::Current();
    const uint32_t never[5] = {rules.Value("t7_to_t8_req_never_true"), rules.Value("t7_to_t8_req_never_true_param"), 0, 0, 0};
    const uint32_t always[5] = {0, 0, 0, 0, 0};
    uint64_t changed = 0;
    for (size_t at = 0; at < records.size(); at += 20) {
        const std::array<uint32_t, 4> params{Read<uint32_t>(records, at + 4), Read<uint32_t>(records, at + 8),
            Read<uint32_t>(records, at + 12), Read<uint32_t>(records, at + 16)};
        const auto holds = ImportProfile::RequirementOverride(profile, ImportProfile::Game::T8, Read<uint32_t>(records, at), params);
        if (!holds) continue;
        memcpy(records.data() + at, *holds ? always : never, 20); ++changed;
    }
    return changed;
}
uint64_t ApplyProfileProperties(std::span<Byte> records, const ImportProfile::Profile& profile) {
    if (records.size() % 0x28) throw std::runtime_error("Invalid retail extra property records");
    uint64_t changed = 0;
    for (size_t at = 0; at < records.size(); at += 0x28) {
        const auto id = Read<uint32_t>(records, at + 16);
        if (!id || !ImportProfile::StripsProperty(profile, ImportProfile::Game::T8, id)) continue;
        const uint32_t frame = ImportProfile::StrippedFrame; // a frame no move reaches: never fires
        memcpy(records.data() + at, &frame, 4); ++changed;
    }
    return changed;
}
uint64_t AdaptCharacterRequirements(std::span<Byte> records, uint32_t source, uint32_t primaryTarget, uint32_t targetSecondary) {
    if (records.size() % 20) throw std::runtime_error("Invalid retail requirement records");
    const uint32_t secondaryTarget = targetSecondary == UINT32_MAX ? primaryTarget : targetSecondary;
    if (source == primaryTarget && source == secondaryTarget) return 0;
    uint64_t changed = 0;
    // Build 26155785, helper RVA 0x187ae80: primary identities accept legacy
    // selector codes as well as direct IDs. Unimplemented selector slots are
    // always false. Secondary identities compare their parameters directly.
    constexpr uint32_t selectors[] = {23,24,33,UINT32_MAX,UINT32_MAX,42,UINT32_MAX,5,
        UINT32_MAX,43,UINT32_MAX,22,UINT32_MAX,14,UINT32_MAX};
    for (size_t at = 0; at < records.size(); at += 20) {
        const auto id = Read<uint32_t>(records, at);
        const bool primary = id == 220 || id == 221 || id == 499 || id == 500;
        const bool secondary = id == 224 || id == 225 || id == 503 || id == 504;
        if (!primary && !secondary) continue; // Opponent predicates stay unchanged.
        const uint32_t target = primary ? primaryTarget : secondaryTarget;
        if (source == target) continue;
        const bool list = id >= 499;
        // An identity the field cannot hold is never written: a record naming the other character keeps its value.
        const uint32_t limit = primary ? 125U : 255U;
        for (size_t member = 0; member < (list ? 4 : 1); ++member) {
            const auto field = at + 4 + member * 4;
            const auto old = Read<uint32_t>(records, field);
            // Scalar forms consume only the low word; preserve other flag bits.
            const uint32_t encoded = list ? old : old & 0xffffU;
            if (list && encoded == (primary ? 125U : 255U)) continue;
            auto decoded = encoded;
            if (primary && encoded >= 0x81 && encoded <= 0x8f) decoded = selectors[encoded - 0x81];
            if (decoded == UINT32_MAX) continue;
            const auto replacement = decoded == source && target < limit ? target
                : decoded == target && source < limit ? source : encoded;
            const auto value = list ? replacement : (old & 0xffff0000U) | replacement;
            if (value != old) { memcpy(records.data() + field,&value,4); ++changed; }
        }
    }
    return changed;
}
Audit AuditImage(std::span<const Byte> payload, const GameProcess& image, gameAddr base, uint64_t size,
    const GameProcess& live, gameAddr originalRoot, uint32_t sourceCharacter, uint32_t targetCharacter, bool convertedT7, const RetailEditorData::Metadata* metadata,
    const std::set<uint32_t>* missingCarriers, const MemoryOnlyAudit* memoryOnly, const ImportProfile::Profile* profile, uint32_t targetSecondary,
    const std::map<std::pair<uint32_t, uint32_t>, std::vector<Byte>>* fallbackCancels) {
    const ImportProfile::Profile importProfile = profile ? *profile : ImportProfile::Profile{};
    R::Validate(payload); Audit audit;
    const auto snapshot = Read<R::Header>(payload, 0);
    auto remote = [](const GameProcess& process, gameAddr address, size_t length) {
        std::vector<Byte> bytes(length);
        if (!process.readBytes(address, bytes.data(), length)) throw std::runtime_error("Cannot read retail image audit region");
        return bytes;
    };
    auto inside = [&](gameAddr address, uint64_t length) {
        if (address < base || address - base > size || length > size - (address - base))
            throw std::runtime_error("Retail audit pointer escaped uploaded allocation");
    };
    inside(base, R::SourceHeaderSize);
    auto header = remote(image, base, R::SourceHeaderSize), current = remote(live, originalRoot, R::SourceHeaderSize);
    std::array<R::Entry, R::Tables.size()> entries;
    std::array<gameAddr, R::Tables.size()> addresses;
    auto expectedHeader = std::vector<Byte>(payload.begin() + snapshot.sourceHeaderOffset,
        payload.begin() + snapshot.sourceHeaderOffset + R::SourceHeaderSize);
    if (convertedT7) memcpy(expectedHeader.data() + 4, current.data() + 4, 4);
    if (convertedT7 && targetCharacter != UINT32_MAX) ApplyConvertedIdentity(expectedHeader, {}, targetCharacter);
    for (size_t i = 0; i < entries.size(); ++i) {
        entries[i] = Read<R::Entry>(payload, snapshot.directoryOffset + i * sizeof(R::Entry));
        const auto& spec = R::Tables[i]; addresses[i] = Read<uint64_t>(header, spec.pointerOffset);
        if (Read<uint64_t>(header, spec.countOffset) != entries[i].count) throw std::runtime_error("Retail audit table count changed");
        if (entries[i].count) inside(addresses[i], entries[i].count * entries[i].stride);
        else if (addresses[i]) throw std::runtime_error("Retail audit empty table has a pointer");
        Write(expectedHeader, spec.pointerOffset, addresses[i]);
    }
    for (size_t field = 0x10; field < 0x30; field += 8) Write(expectedHeader, field, Read<uint64_t>(current, field));
    Write(expectedHeader, 0x170, Read<uint64_t>(current, 0x170));
    for (size_t field = 0x2b0; field <= 0x310; field += 8) Write(expectedHeader, field, Read<uint64_t>(current, field));
    if (expectedHeader != header) throw std::runtime_error("Retail header audit differs from export and runtime bindings");
    audit.bytes += header.size();
    for (size_t table = 0; table < entries.size(); ++table) {
        const auto& entry = entries[table]; if (!entry.count) continue;
        auto expected = std::vector<Byte>(payload.begin() + entry.offset, payload.begin() + entry.offset + entry.count * entry.stride);
        if (table == 1 && sourceCharacter != UINT32_MAX && targetCharacter != UINT32_MAX)
            AdaptCharacterRequirements(expected, ProfileIdentity(importProfile, sourceCharacter), targetCharacter, targetSecondary);
        if (table == 1) ApplyProfileConditions(expected, importProfile);
        if (table == 9) ApplyProfileProperties(expected, importProfile);
        if (table == 12 && convertedT7 && targetCharacter != UINT32_MAX) {
            std::vector<Byte> scratch(R::SourceHeaderSize);
            ApplyConvertedIdentity(scratch, expected, targetCharacter);
        }
        auto actual = remote(image, addresses[table], expected.size());
        for (const auto& field : PointerFields) {
            if (field.table != table) continue;
            const auto& target = entries[field.target];
            for (size_t row = 0; row < entry.count; ++row) {
                const auto offset = row * entry.stride + field.offset;
                auto pointer = Read<uint64_t>(expected, offset);
                if (table == 12 && (field.offset == 0xa0 || field.offset == 0xb0 || field.offset == 0xc0) &&
                    Read<int32_t>(expected, offset + 8) <= 0) pointer = 0;
                if (!pointer) { Write(expected, offset, 0); continue; }
                if (pointer >= target.address && pointer - target.address < target.count * target.stride &&
                    (pointer - target.address) % target.stride == 0) {
                    Write(expected, offset, addresses[field.target] + pointer - target.address); continue;
                }
                if (table != 12 || field.target != 6) throw std::runtime_error("Unresolved retail audit reference");
                auto destination = Read<uint64_t>(actual, offset);
                const auto origin = metadata ? metadata->origins[12].at(row) : row;
                // Target fallback (ImporterT8::FallbackCancels): the list the importer built, final bytes.
                if (fallbackCancels) {
                    const auto found = fallbackCancels->find({static_cast<uint32_t>(origin), field.offset});
                    if (found == fallbackCancels->end()) throw std::runtime_error("Unrecorded fallback audit cancel list");
                    inside(destination, found->second.size());
                    if (remote(image, destination, found->second.size()) != found->second) throw std::runtime_error("Retail fallback cancel audit mismatch");
                    audit.bytes += found->second.size(); ++audit.externalLists; Write(expected, offset, destination);
                    continue;
                }
                // Memory-only native imports take the list (and its table bases) from the file's capture.
                const std::vector<Byte>* captured = nullptr;
                if (memoryOnly && memoryOnly->cancels) {
                    const auto found = memoryOnly->cancels->find({static_cast<uint32_t>(origin), field.offset});
                    if (found == memoryOnly->cancels->end()) throw std::runtime_error("Uncaptured external audit cancel list");
                    captured = &found->second;
                }
                const auto external = captured ? 0 : Read<uint64_t>(remote(live, Read<uint64_t>(current, 0x230) + origin * 0x448, 0x448), field.offset);
                bool terminated = false;
                auto sourceTable = [&](size_t targetTable) {
                    const auto start = captured ? entries[targetTable].address : Read<uint64_t>(current, R::Tables[targetTable].pointerOffset);
                    const auto count = !captured ? Read<uint64_t>(current, R::Tables[targetTable].countOffset)
                        : metadata ? Read<uint64_t>(std::span<const Byte>(metadata->source), R::Tables[targetTable].countOffset) : entries[targetTable].count;
                    return std::pair<uint64_t, uint64_t>{start, count};
                };
                auto place = [&](size_t targetTable, uint64_t id) { return addresses[targetTable] + id * entries[targetTable].stride; };
                for (size_t index = 0, kept = 0; index < 4096; ++index) {
                    if (captured && (index + 1) * 40 > captured->size()) break;
                    auto read = captured ? std::vector<Byte>(captured->begin() + index * 40, captured->begin() + (index + 1) * 40)
                                         : remote(live, external + index * 40, 40);
                    auto wanted = RebaseExternalCancel(std::move(read), sourceTable, entries, metadata, place);
                    if (!wanted) continue; // dropped by the importer: its dependency was deleted
                    inside(destination + kept * 40, 40);
                    if (remote(image, destination + kept * 40, 40) != *wanted) throw std::runtime_error("Retail external cancel audit mismatch");
                    ++kept; audit.bytes += 40;
                    if (Read<uint64_t>(*wanted, 0) == 0x8000) { terminated = true; break; }
                }
                if (!terminated) throw std::runtime_error("Unterminated retail audit cancel list");
                ++audit.externalLists; Write(expected, offset, destination);
            }
        }
        if (table == 12) {
            const auto moveCount = Read<uint64_t>(current, 0x238);
            if (!moveCount || moveCount > (256ULL << 20) / 0x448) throw std::runtime_error("Invalid audit binding move count");
            auto loaded = remote(live, Read<uint64_t>(current, 0x230), moveCount * 0x448);
            std::vector<size_t> animations;
            if (memoryOnly) {
                if (memoryOnly->handles.size() != entry.count || memoryOnly->stringRow >= moveCount)
                    throw std::runtime_error("Invalid memory-only audit inputs");
                animations.assign(entry.count, memoryOnly->stringRow);
            } else animations = BindAnimations(R::Table(payload, 12), loaded, convertedT7, metadata, Read<uint16_t>(current, 0xaa));
            if (missingCarriers) FallBackMissingCarriers(animations, payload, *missingCarriers);
            for (size_t row = 0; row < entry.count; ++row)
                for (auto field : {0x40, 0x48, 0x50}) {
                    const auto hash=Name(expected,row*0x448);
                    if(field==0x40 && metadata && metadata->names.contains(hash)){
                        const auto& name=metadata->names.at(hash);auto address=Read<uint64_t>(actual,row*0x448+field);inside(address,name.size()+1);
                        auto string=remote(image,address,name.size()+1);if(memcmp(string.data(),name.c_str(),name.size()+1))throw std::runtime_error("Custom move name audit differs");
                        Write(expected,row*0x448+field,address);
                    }else if(field!=0x50 && memoryOnly && row<memoryOnly->names.size() && (field==0x40?memoryOnly->names:memoryOnly->animations)[row]){
                        // Localised native move string: inside the allocation, with the captured text.
                        const auto& text=*(field==0x40?memoryOnly->names:memoryOnly->animations)[row];
                        auto address=Read<uint64_t>(actual,row*0x448+field);inside(address,text.size()+1);
                        auto string=remote(image,address,text.size()+1);
                        if(memcmp(string.data(),text.c_str(),text.size()+1))throw std::runtime_error("Localised move string audit differs");
                        Write(expected,row*0x448+field,address);
                    }else{
                        auto origin=metadata?metadata->origins[12].at(row):row;
                        Write(expected,row*0x448+field,Read<uint64_t>(loaded,(field==0x40 && !convertedT7 && !memoryOnly && origin<moveCount?origin:animations[row])*0x448+field));
                    }
                }
            if (memoryOnly) {
                std::set<uint64_t> checked;
                // Every body handle must resolve: no exception for animations the game had not loaded at export (the
                // importer reads those from tkdata.bin, it never keeps an unresolved handle).
                for (size_t row = 0; row < entry.count; ++row) {
                    const auto handle = memoryOnly->handles[row];
                    // Qualified file handles become ours; unqualified ones keep the target row's value written above.
                    if (Read<uint64_t>(R::Table(payload, 12), row * 0x448 + 0x50) >> 32) Write(expected, row * 0x448 + 0x50, handle);
                    if (!(handle >> 32) || !checked.insert(handle).second) continue;
                    if (((handle >> 40) & 0xff) != (memoryOnly->domain & 0xff)) throw std::runtime_error("Memory-only handle has another domain");
                    // Each body handle must resolve, for the game's readers, to a finished resource with its animation. The
                    // game's resolver (0x145A9ADD0, docs: retail-anim-lookup) takes the shared domain 0x81 only when its
                    // entry is ready, else the character's domain: a declared but unloaded shared entry falls through.
                    const bool resolved = ReadyAnimation(image, memoryOnly->container, memoryOnly->domain, static_cast<uint32_t>(handle), memoryOnly->compiledVtable) != 0;
                    if (!resolved && memoryOnly->dataless && memoryOnly->dataless->count(static_cast<uint32_t>(handle))) continue;
                    if (!resolved)
                        throw std::runtime_error("Memory-only animation does not resolve to a finished resource (move " + std::to_string(row) + ", handle " + std::format("{:016X}", handle) + ")");
                    ++audit.animations;
                }
            }
        }
        if (expected != actual) {
            size_t at = 0; while (at < expected.size() && expected[at] == actual[at]) ++at;
            throw std::runtime_error(std::string("Retail image audit mismatch: ") + R::Tables[table].name + " row " +
                std::to_string(at / entry.stride) + " field 0x" + [&]{ char text[16]; snprintf(text, sizeof(text), "%zx", at % entry.stride); return std::string(text); }());
        }
        audit.bytes += actual.size();
    }
    return audit;
}
}
