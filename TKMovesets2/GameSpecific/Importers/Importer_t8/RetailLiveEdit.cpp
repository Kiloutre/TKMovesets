#include "RetailLiveEdit.hpp"
#include "RetailImport.hpp"
#include "Extractor_t8/RetailSnapshot.hpp"
#include "Editor_t8Retail/RetailEditorData.hpp"
#include "Editor_t8Retail/Editor_t8Retail.hpp"
#include "Importer_t8.hpp"
#include <optional>
#include "MovesetStructs.h"
#include "GameIDs.hpp"
#include <array>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace RetailLiveEdit {
namespace {
namespace R = RetailT8Snapshot;
template<class T> T Get(std::span<const Byte> b, size_t at) {
    if (at > b.size() || sizeof(T) > b.size() - at) throw std::runtime_error("Truncated retail live-edit field");
    T v; memcpy(&v, b.data() + at, sizeof(T)); return v;
}
struct View {
    const TKMovesetHeader* outer = nullptr;
    std::span<const Byte> payload;
    R::Header header{};
    std::array<R::Entry, 20> entries{};
    std::span<const Byte> source;
};
View Open(std::span<const Byte> file) {
    if (file.size() < sizeof(TKMovesetHeader)) throw std::runtime_error("Missing retail container");
    View v; v.outer = reinterpret_cast<const TKMovesetHeader*>(file.data());
    if (v.outer->moveset_data_start > file.size()) throw std::runtime_error("Invalid retail container");
    v.payload = file.subspan(v.outer->moveset_data_start);
    R::Validate(v.payload);
    v.header = Get<R::Header>(v.payload, 0);
    for (size_t i = 0; i < 20; ++i) v.entries[i] = Get<R::Entry>(v.payload, v.header.directoryOffset + i * sizeof(R::Entry));
    v.source = v.payload.subspan(v.header.sourceHeaderOffset, R::SourceHeaderSize);
    return v;
}
std::span<const Byte> Row(const View& v, size_t table, uint64_t row) {
    const auto& e = v.entries[table];
    return v.payload.subspan(e.offset + row * e.stride, e.stride);
}
std::optional<uint32_t> AnimationHash(std::span<const Byte> move) {
    return EditorT8Retail::Decode(Get<uint64_t>(move, 0x20), Get<uint64_t>(move, 0x28));
}
bool SameMetadata(const View& a, const View& b) {
    auto ma = RetailEditorData::Read(*a.outer, a.payload), mb = RetailEditorData::Read(*b.outer, b.payload);
    return ma.stored == mb.stored && ma.names == mb.names && ma.moveNames == mb.moveNames && ma.origins == mb.origins && ma.remaps == mb.remaps;
}
// Imported PANMs (custom assets) are compiled and attached only by a full import.
bool SameAssets(const View& a, const View& b) {
    auto x = R::Assets(a.payload), y = R::Assets(b.payload);
    if (x.size() != y.size()) return false;
    for (size_t i = 0; i < x.size(); ++i)
        if (x[i].address != y[i].address || x[i].size != y[i].size || x[i].instance != y[i].instance || x[i].type != y[i].type ||
            memcmp(a.payload.data() + x[i].offset, b.payload.data() + y[i].offset, x[i].size)) return false;
    return true;
}
}

Result Sync(GameProcess& process, gameAddr player, gameAddr root, std::span<const Byte> previous, std::span<const Byte> current,
    const RetailT8Runtime::PlayerOffsets& offsets) {
    Result result;
    auto structural = [&](const char* why) { result.structural = true; result.reason = why; return result; };
    const auto a = Open(previous), b = Open(current);
    if (a.outer->characterId != b.outer->characterId) return structural("character identity changed");
    const bool convertedT7 = b.outer->GetConversionOrigin() == GameVersions::T8::FROM_T7;
    if (convertedT7 != (a.outer->GetConversionOrigin() == GameVersions::T8::FROM_T7)) return structural("conversion origin changed");
    for (size_t i = 0; i < 20; ++i)
        if (a.entries[i].count != b.entries[i].count || a.entries[i].stride != b.entries[i].stride || a.entries[i].address != b.entries[i].address)
            return structural("table layout changed");
    if (memcmp(a.source.data(), b.source.data(), a.source.size())) return structural("source header changed");
    if (!SameMetadata(a, b)) return structural("move names or row provenance changed");
    if (!SameAssets(a, b)) return structural("animation assets changed");

    // The published allocation must still be this player's moveset.
    uint64_t installed = 0;
    if (!process.readBytes(player + offsets.moveset, &installed, 8) || installed != root) return structural("published moveset is no longer installed");
    std::vector<Byte> live(R::SourceHeaderSize);
    if (!process.readBytes(root, live.data(), live.size())) return structural("cannot read published moveset");
    std::array<gameAddr, 20> bases{};
    for (size_t i = 0; i < 20; ++i) {
        const auto count = Get<uint64_t>(live, R::Tables[i].countOffset);
        if (count != b.entries[i].count) return structural("published table counts differ");
        bases[i] = Get<uint64_t>(live, R::Tables[i].pointerOffset);
        if (count && !bases[i]) return structural("published table is missing");
    }
    uint32_t target = 0;
    if (!process.readBytes(player + offsets.character, &target, 4)) return structural("cannot read player identity");
    uint32_t targetSecondary = 0;
    if (!process.readBytes(player + offsets.character + 4, &targetSecondary, 4)) return structural("cannot read player identity");
    // The importer gives converted T7 move rows the target's identity codes (+0xd0..; row-parity dependent):
    // edited rows must carry them too, never the converter's placeholder.
    std::vector<Byte> convertedMoves;
    if (convertedT7) {
        const auto moves = b.payload.subspan(b.entries[12].offset, b.entries[12].count * b.entries[12].stride);
        convertedMoves.assign(moves.begin(), moves.end());
        std::vector<Byte> scratch(R::SourceHeaderSize);
        RetailT8Import::ApplyConvertedIdentity(scratch, convertedMoves, target);
    }

    struct Write { gameAddr address; std::vector<Byte> bytes; };
    std::vector<Write> writes;
    for (size_t table = 0; table < 20; ++table) {
        const auto& e = b.entries[table];
        for (uint64_t row = 0; row < e.count; ++row) {
            const auto before = Row(a, table, row), now = Row(b, table, row);
            if (!memcmp(before.data(), now.data(), now.size())) continue;
            const gameAddr address = bases[table] + row * e.stride;
            std::vector<Byte> old(e.stride), next(now.begin(), now.end());
            if (convertedT7 && table == 12) next.assign(convertedMoves.begin() + row * e.stride, convertedMoves.begin() + (row + 1) * e.stride);
            if (!process.readBytes(address, old.data(), old.size())) return structural("cannot read published record");
            for (const auto& field : RetailT8Import::PointerFields) {
                if (field.table != table) continue;
                const auto value = Get<uint64_t>(now, field.offset);
                const auto& t = b.entries[field.target];
                uint64_t relocated;
                const bool inside = value >= t.address && value - t.address < t.count * t.stride && (value - t.address) % t.stride == 0;
                // Inactive optional cancel banks are cleared by the importer.
                if (table == 12 && (field.offset == 0xa0 || field.offset == 0xb0 || field.offset == 0xc0) && Get<int32_t>(now, field.offset + 8) <= 0)
                    relocated = 0;
                else if (!value) {
                    // Same graph rules as RetailT8Import::Prepare: a full import reports these.
                    if (table == 14 && Get<uint16_t>(now, 2)) return structural("input sequence has no input pointer");
                    relocated = 0;
                }
                else if (inside) {
                    if (table == 14 && Get<uint16_t>(now, 2) > t.count - (value - t.address) / t.stride)
                        return structural("input sequence extends beyond input table");
                    relocated = bases[field.target] + (value - t.address);
                }
                // External list bound at import: only while the document still holds the same reference.
                else if (value == Get<uint64_t>(before, field.offset)) relocated = Get<uint64_t>(old, field.offset);
                else return structural("reference is outside its table");
                memcpy(next.data() + field.offset, &relocated, 8);
            }
            if (table == 12) {
                // Engine-bound name/animation handles stay as published, unless the
                // animation identity (0x20) changed: then take the handles of a published
                // row that already plays that animation (SetAnimation copies from a donor row).
                memcpy(next.data() + 0x40, old.data() + 0x40, 8);
                const auto animation = AnimationHash(now);
                gameAddr source = address;
                if (animation != AnimationHash(before)) {
                    source = 0;
                    for (uint64_t other = 0; other < a.entries[12].count && !source; ++other)
                        if (AnimationHash(Row(a, 12, other)) == animation) source = bases[12] + other * e.stride;
                    if (!source) return structural("move animation has no published binding");
                }
                std::array<Byte, 16> handles{};
                if (source == address) memcpy(handles.data(), old.data() + 0x48, 16);
                else if (!process.readBytes(source + 0x48, handles.data(), 16)) return structural("cannot read published animation");
                memcpy(next.data() + 0x48, handles.data(), 16);
            }
            if (table == 1) {
                const auto profile = ImportProfile::Read(*b.outer);
                RetailT8Import::AdaptCharacterRequirements(next, RetailT8Import::ProfileIdentity(profile, b.outer->characterId), target, targetSecondary);
                RetailT8Import::ApplyProfileConditions(next, profile);
            }
            if (table == 9) RetailT8Import::ApplyProfileProperties(next, ImportProfile::Read(*b.outer));
            if (next == old) continue;
            result.bytes += next.size(); ++result.records;
            writes.push_back({address, std::move(next)});
        }
    }
    if (writes.empty()) return result;
    // All records or none, and only while this allocation is still the player's moveset.
    GameProcess::WriteBatch transaction(process);
    if (!process.ExpectBytes(player + offsets.moveset, &root, sizeof(root))) throw std::runtime_error("Cannot guard live edit");
    for (const auto& w : writes)
        if (!process.writeBytes(w.address, w.bytes.data(), w.bytes.size()))
            throw std::runtime_error("Live edit write failed");
    GameProcess::PauseGuard pause(process);
    const auto committed = transaction.Commit();
    if (!committed.ok) throw std::runtime_error(committed.rollbackComplete ? "Live edit was not applied (game state changed or write failed)" : "Live edit rollback failed");
    return result;
}

Published Apply(ImporterT8& importer, GameProcess& process, uint8_t slot,
    gameAddr boundRoot, const std::vector<Byte>* boundSnapshot, std::span<const Byte> document) {
    Published out;
    if (boundRoot && boundSnapshot) {
        out.sync = Sync(process, importer.GetCharacterAddress(slot), boundRoot, *boundSnapshot, document, importer.Offsets());
        if (!out.sync.structural) { out.root = boundRoot; out.direct = true; return out; }
    }
    uint8_t progress = 0;
    const auto code = importer.Import(document.data(), document.size(), importer.GetCharacterAddress(slot), ImportSettings_ApplyInstantly, progress);
    if (code != ImportationErrcode_Successful)
        throw std::runtime_error(importer.LastError().empty() ? "Document import failed; game state changed or write failed" : importer.LastError());
    out.root = importer.lastLoaded.address;
    return out;
}
}
