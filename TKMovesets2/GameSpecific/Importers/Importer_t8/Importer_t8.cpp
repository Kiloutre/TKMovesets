#include "Importer_t8.hpp"
#include "Extractor_t8/RetailGameData.hpp"
#include "RetailCommands.hpp"
#include "RetailImport.hpp"
#include "Extractor_t8/RetailRuntime.hpp"
#include "GameIDs.hpp"
#include "RetailResourceLoader.hpp"
#include "T7ToRetailT8.hpp"
#include "T7AnimToRetailT8.hpp"
#include "T7HandToRetailT8.hpp"
#include "T7CameraToRetailT8.hpp"
#include "T7WingToRetailT8.hpp"
#include "T7FaceToRetailT8.hpp"
#include "Editor_t8Retail/Editor_t8Retail.hpp"
#include <bit>
#include <chrono>
#include <map>
#include <optional>
#include <set>
#include <algorithm>
#include <cstring>

namespace {
namespace R = RetailT8Snapshot;
namespace RT = RetailT8Runtime;
template<class T> T At(std::span<const Byte> bytes, size_t offset) {
    if (offset > bytes.size() || sizeof(T) > bytes.size() - offset) throw std::runtime_error("Truncated retail import field");
    T result; memcpy(&result, bytes.data() + offset, sizeof(T)); return result;
}
void Put(std::vector<Byte>& bytes, size_t offset, uint64_t value) { memcpy(bytes.data() + offset, &value, 8); }
std::vector<Byte> Read(const GameProcess& process, gameAddr address, size_t size) {
    std::vector<Byte> bytes(size);
    if (!RT::Pointer(address) || size > 0x800000000000ULL - address || !process.probeBytes(address, bytes.data(), bytes.size()))
        throw std::runtime_error("Cannot read retail import dependency");
    return bytes;
}
}

gameAddr ImporterT8::GetCharacterAddress(uint8_t slot) {
    if (slot >= 2) return 0;
    try {
        auto base = m_process.mainModule.address;
        return RT::Players(m_process, base + m_game.GetValue("registry_rva"), base + m_game.GetValue("round_key_rva"))[slot];
    } catch (const std::exception&) { return 0; }
}
gameAddr ImporterT8::GetMovesetAddress(uint8_t slot) {
    auto player = GetCharacterAddress(slot);
    if (!player) return 0;
    try { return RT::Read<uint64_t>(m_process, player + m_game.GetValue("motbin_offset")); }
    catch (const std::exception&) { return 0; }
}
bool ImporterT8::CanImport() {
    for (uint8_t slot = 0; slot < 2; ++slot) if (RT::Pointer(GetMovesetAddress(slot))) return true;
    return false;
}
std::vector<Byte> ImporterT8::ConvertT7(const TKMovesetHeader& outer, const StructsT7::TKMovesetHeaderBlocks& blocks,
    std::span<const Byte> t7Payload, gameAddr) {
    // Self-contained conversion: no other moveset, no files, no live capture; the same output for every target.
    return T7ToRetailT8::ConvertStandalone(outer, blocks, t7Payload, nullptr, true, ImportProfile::Read(outer));
}

ImportationErrcode_ ImporterT8::ImportMovesetData(const TKMovesetHeader* outer, Byte* raw, uint64_t size,
    gameAddr player, ImportSettings settings, uint8_t& progress) {
    m_error.clear();
    m_bindingRoot = 0; m_characterFixes = 0;
    m_timings.clear();
    auto phaseStart = std::chrono::steady_clock::now();
    auto phase = [&](const char* name) {
        const auto now = std::chrono::steady_clock::now();
        m_timings.push_back({name, std::chrono::duration<double, std::milli>(now - phaseStart).count()});
        phaseStart = now;
    };
    try {
        std::vector<Byte> convertedFile;
        if (outer->gameId == GameId_T7) {
            if (outer->block_list_size < 8) throw std::runtime_error("T7 block table is truncated");
            StructsT7::TKMovesetHeaderBlocks blocks;
            memcpy(&blocks, reinterpret_cast<const Byte*>(outer) + outer->block_list, sizeof(blocks));
            convertedFile = ConvertT7(*outer, blocks, {raw, static_cast<size_t>(size)}, player);
            outer = reinterpret_cast<const TKMovesetHeader*>(convertedFile.data());
            raw = convertedFile.data() + outer->moveset_data_start;
            size = convertedFile.size() - outer->moveset_data_start;
        }
        if (outer->gameId != GameId_T8 || outer->minorVersion != GameVersions::T8::RETAIL)
            return ImportationErrcode_UnsupportedGameVersion;
        if (settings & (ImportSettings_BasicLoadOnly | ImportSettings_ImportOriginalData))
            throw std::runtime_error("Retail import requires a loaded target and does not support online/basic loading");
        const bool convertedT7 = outer->GetConversionOrigin() == GameVersions::T8::FROM_T7;
        bool found = false;
        for (uint8_t slot = 0; slot < 2; ++slot) if (GetCharacterAddress(slot) == player && player) found = true;
        if (!found) throw std::runtime_error("Retail target player/build is no longer available");
        const auto offsets = Offsets();
        const auto oldRoot = RT::Read<uint64_t>(m_process, player + offsets.moveset);
        const auto targetCharacter = RT::Read<uint32_t>(m_process, player + offsets.character);
        const auto secondary = RT::Read<uint32_t>(m_process, player + offsets.character + 4);
        // Self-character checks are rewritten per field: primary checks to +0x168, secondary checks to +0x16c.
        const std::span<const Byte> payload(raw, static_cast<size_t>(size));
        R::Validate(payload);
        auto provenance=RetailEditorData::Read(*outer,payload);
        const auto* metadata=provenance.stored?&provenance:nullptr;
        const auto animationCharacter = RT::AnimationCharacter(R::Table(payload, 12), outer->characterId);
        const auto snapshot = At<R::Header>(payload, 0);
        const auto source = payload.subspan(snapshot.sourceHeaderOffset, R::SourceHeaderSize);
        std::array<R::Entry, R::Tables.size()> entries;
        for (size_t i = 0; i < entries.size(); ++i)
            entries[i] = At<R::Entry>(payload, snapshot.directoryOffset + i * sizeof(R::Entry));
        // Prefer active matching roots. The original capture root is also a
        // candidate for restoring a replaced moveset, but is never trusted by
        // address alone: date, counts, every name and all PANMs are checked.
        std::vector<std::pair<gameAddr, gameAddr>> candidates{{oldRoot, player}};
        for (uint8_t slot = 0; slot < 2; ++slot) {
            auto owner = GetCharacterAddress(slot);
            if (owner && owner != player) candidates.emplace_back(RT::Read<uint64_t>(m_process, owner + offsets.moveset), owner);
        }
        if (!convertedT7) candidates.emplace_back(snapshot.sourceAddress, 0);
        // Memory-only route (.docs/retail-t8-memory-only-import.md): a converted T7 file whose embedded PANMs cover its
        // idle move registers every animation itself, from the file, in the target's own animation map. It needs no
        // binding to a loaded moveset, no carrier, no bridge DLL and no engine call: only memory writes in this batch.
        m_memoryOnly = false; m_publishedAnimations = 0; m_reusedAnimations = 0; m_memoryOnlyFallbackMoves = 0;
        m_memoryOnlyHandles.clear(); m_memoryOnlyStringRow = 0; m_memoryOnlyDomain = 0; m_memoryOnlyContainer = 0;
        m_nativeBoundAnimations = 0; m_nativeUnloadedKept = 0; m_native.reset(); m_memoryOnlyNames.clear(); m_memoryOnlyAnimations.clear();
        m_handIndices.clear(); m_handAnimations = 0; m_publishedHandIndices = 0; m_silencedHandProperties = 0;
        // v4 native snapshots carry their source's strings and external cancel lists (.docs/retail-t8-extraction.md).
        if (!convertedT7) try { m_native = R::ReadExtension(payload); } catch (const std::exception&) { m_native.reset(); }
        std::map<uint32_t, R::AssetEntry> bodyAssets; // file instance -> PANM (converted ones win over embedded natives)
        // Every animation a move plays is imported. One the file does not carry (the game had not loaded it at export,
        // or an older file) is read from the game's own tkdata.bin, the bytes the game itself loads. One found nowhere
        // plays the target's idle animation (counted in MemoryOnlyFallbackMoves).
        const auto tkdataPath = RetailT8GameData::TkdataPath(m_process.mainModule.path);
        RetailT8GameData::Source disk(tkdataPath, [this, tkdataPath](uint32_t domain) {
            // the archive's own roster (motbin header identities) first, then the names file
            if (auto code = RetailT8GameData::TkdataCodeFor(tkdataPath, domain); !code.empty()) return code;
            const auto character = m_names->FindCharacter(domain);
            return character ? character->code : std::string();
        });
        std::map<uint32_t, std::vector<Byte>> diskAssets; // file instance -> PANM read from tkdata.bin
        m_unavailableAnimations.clear(); m_dataless.clear(); // declared by the game's archives without any data: the game has nothing to load for them
        m_archiveError.clear();
        auto fromDisk = [&](uint64_t handle, size_t row) {
            const auto instance = static_cast<uint32_t>(handle);
            if (bodyAssets.count(instance) || diskAssets.count(instance) || m_dataless.count(instance) ||
                m_unavailableAnimations.count(instance)) return;
            try {
                if (auto panm = disk.Panm(static_cast<uint32_t>((handle >> 40) & 0xff), 0, instance)) diskAssets[instance] = std::move(*panm);
                else m_dataless.insert(instance);
            }
            // Neither the file nor tkdata.bin has it: its moves play the idle animation (see the binding below).
            // The first error is kept for the report: it may be environmental (no tkdata.bin, no archive code for the
            // character because TKMInterface/t8-retail_names.json is missing) rather than a genuinely absent animation.
            catch (const std::exception& error) {
                DEBUG_ERR("Retail T8 import: animation %u (move %zu) not found in tkdata.bin: %s", instance, row, error.what());
                if (m_archiveError.empty()) m_archiveError = error.what();
                m_unavailableAnimations.insert(instance);
            }
        };
        std::vector<Byte> targetHeader, targetMoves;
        gameAddr animContainer = 0, targetMap = 0, targetMapSlot = 0;
        if (convertedT7 || m_native) try {
            for (const auto& asset : R::Assets(payload))
                if (asset.type == 0 && asset.size) {
                    if (asset.address == T7AnimToRetailT8::CustomAssetAddress) bodyAssets[asset.instance] = asset;
                    else bodyAssets.try_emplace(asset.instance, asset);
                }
            const auto fileMoves = R::Table(payload, 12);
            const auto fileIdle = At<uint16_t>(source, 0xaa);
            if (fileIdle < fileMoves.size() / 0x448 && (m_native || !bodyAssets.empty())) {
                targetHeader = Read(m_process, oldRoot, R::SourceHeaderSize);
                const auto count = At<uint64_t>(targetHeader, 0x238);
                // A native snapshot must come from this game build (same header compilation date).
                if (count && count <= (256ULL << 20) / 0x448 && At<uint16_t>(targetHeader, 0xaa) < count &&
                    (convertedT7 || At<uint32_t>(source, 4) == At<uint32_t>(targetHeader, 4))) {
                    targetMoves = Read(m_process, At<uint64_t>(targetHeader, 0x230), count * 0x448);
                    m_memoryOnlyDomain = RT::AnimationCharacter(targetMoves, targetCharacter);
                    animContainer = m_game.ReadPtrPath("anim_container");
                    targetMapSlot = RT::DomainMapSlot(m_process, animContainer, m_memoryOnlyDomain);
                    targetMap = targetMapSlot ? RT::Read<uint64_t>(m_process, targetMapSlot) : 0;
                    // Every animation comes from the file or the game's archive (checked per move before any write).
                    m_memoryOnly = targetMap && RT::Read<uint8_t>(m_process, targetMap + 0x48) == 1;
                    // The entries point at game objects listed in game_addresses.ini; without them this route is off.
                    if (m_memoryOnly) try { Engine(); } catch (const std::exception& error) {
                        DEBUG_ERR("Retail T8 import: memory-only route unavailable: %s", error.what()); m_memoryOnly = false;
                    }
                    // Every external cancel list the move table points at must have been captured.
                    if (m_memoryOnly && m_native) {
                        const auto& cancels = entries[6];
                        for (size_t row = 0; m_memoryOnly && row < fileMoves.size() / 0x448; ++row)
                            for (auto field : R::ExternalCancelFields) {
                                const auto pointer = At<uint64_t>(fileMoves, row * 0x448 + field);
                                if (!pointer || (field != 0x98 && At<int32_t>(fileMoves, row * 0x448 + field + 8) <= 0)) continue;
                                if (pointer >= cancels.address && pointer - cancels.address < cancels.count * cancels.stride &&
                                    (pointer - cancels.address) % cancels.stride == 0) continue;
                                const auto origin = metadata ? metadata->origins[12].at(row) : row;
                                if (!m_native->cancels.count({static_cast<uint32_t>(origin), field})) { m_memoryOnly = false; break; }
                            }
                    }
                }
            }
        } catch (const std::exception&) { m_memoryOnly = false; }
        // Converted T7 files embed every animation and normally play through memory-only registration. Without it
        // (the target's animation map is not ready) they take the binding route below, or the target fallback.
        gameAddr root = 0, bindingPlayer = 0;
        std::vector<Byte> live, moves;
        std::vector<size_t> animationRows;
        std::string bindingError;
        m_bindingIdentityMismatches = 0; m_bindingFallbackMoves = 0; m_droppedExternalCancels = 0; m_editedAnimationsKept = 0;
        m_targetFallback = false; m_fallbackCancels.clear(); m_fallbackCancelLists = 0;
        // Binding route: a loaded moveset provides what the file does not carry (strings, resources, external cancel lists): its tables
        // must have the export's shape. Among those, the one closest to the export wins; differences are not fatal.
        auto selectSource = [&] {
            uint64_t best = UINT64_MAX;
            for (auto [candidate, owner] : candidates) {
            try {
                auto header = Read(m_process, candidate, R::SourceHeaderSize);
                if (!convertedT7 && At<uint32_t>(source, 4) != At<uint32_t>(header, 4)) { bindingError = "a loaded moveset is from another game build"; continue; }
                bool compatible = true;
                for (size_t i = 0; i < entries.size(); ++i)
                    if (!convertedT7 && (metadata ? At<uint64_t>(metadata->source,R::Tables[i].countOffset) : entries[i].count) != At<uint64_t>(header, R::Tables[i].countOffset)) compatible = false;
                if (!compatible) { bindingError = "no loaded moveset has the export's original table sizes"; continue; }
                const auto moveCount = At<uint64_t>(header, 0x238);
                if (!moveCount || moveCount > (256ULL << 20) / 0x448) continue;
                auto records = Read(m_process, At<uint64_t>(header, 0x230), moveCount * 0x448);
                if (convertedT7 && RT::AnimationCharacter(records, animationCharacter) != animationCharacter) continue;
                RetailT8Import::BindStats stats;
                auto bindings = RetailT8Import::BindAnimations(R::Table(payload, 12), records, convertedT7, metadata, At<uint16_t>(header, 0xaa), &stats);
                const auto score = stats.missingAnimations * 0x10000 + stats.identityMismatches;
                if (score >= best) continue;
                best = score; animationRows = std::move(bindings);
                m_bindingIdentityMismatches = stats.identityMismatches; m_bindingFallbackMoves = stats.missingAnimations;
                root = candidate; bindingPlayer = owner; live = std::move(header); moves = std::move(records);
                if (!score) break;
            } catch (const std::exception& error) { bindingError = error.what(); }
        } };
        if (m_memoryOnly) {
            // The target's own moveset provides header strings/resource slots and valid move strings (its idle row).
            root = oldRoot; live = targetHeader; moves = targetMoves;
            m_memoryOnlyStringRow = At<uint16_t>(targetHeader, 0xaa);
            animationRows.assign(R::Table(payload, 12).size() / 0x448, m_memoryOnlyStringRow);
        } else selectSource();
        // Without memory-only registration the moves play the bound moveset's own animations, so its character's
        // animations must be loaded. When no loaded moveset fits, or its animations are not loaded, the target's own
        // moveset is the binding: it is loaded by definition. Moves play the target's animation of the same hash, else
        // its idle (counted in BindingFallbackMoves). A valid moveset is always imported.
        if (!m_memoryOnly) {
            bool haveResources = false;
            if (root) try { haveResources = RT::HasCharacterAnimations(m_process, m_game.ReadPtrPath("anim_container"), animationCharacter); }
            catch (const std::exception&) {}
            if (!root || !haveResources) {
                if (!bindingError.empty()) DEBUG_LOG("Retail T8 import: target fallback (%s)\n", bindingError.c_str());
                auto header = Read(m_process, oldRoot, R::SourceHeaderSize);
                const auto count = At<uint64_t>(header, 0x238);
                if (!count || count > (256ULL << 20) / 0x448) throw std::runtime_error("The target player has no readable moveset");
                moves = Read(m_process, At<uint64_t>(header, 0x230), count * 0x448);
                live = std::move(header); root = oldRoot; bindingPlayer = 0;
                RetailT8Import::BindStats stats;
                animationRows = RetailT8Import::BindAnimations(R::Table(payload, 12), moves, convertedT7, metadata, At<uint16_t>(live, 0xaa), &stats);
                m_bindingIdentityMismatches = stats.identityMismatches; m_bindingFallbackMoves = stats.missingAnimations;
                m_targetFallback = true;
            }
        }
        m_bindingRoot = root;
        const auto registrySlot = m_process.mainModule.address + m_game.GetValue("registry_rva");
        const auto keySlot = m_process.mainModule.address + m_game.GetValue("round_key_rva");
        const auto registry = RT::Read<uint64_t>(m_process, registrySlot);
        const auto roundKey = RT::Read<uint32_t>(m_process, keySlot);
        const auto roundMap = RT::Read<uint64_t>(m_process, registry + 0x10);
        const auto roundNode = RT::Lookup(m_process, RT::Read<uint64_t>(m_process, roundMap + 0x100),
            RT::Read<uint64_t>(m_process, roundMap + 0x110), RT::Read<uint64_t>(m_process, roundMap + 0x128), roundKey, false);
        if (!roundNode) throw std::runtime_error("Retail round is no longer available");
        const auto round = RT::Read<uint64_t>(m_process, roundNode + 0x18);
        if (!m_process.ExpectBytes(registrySlot, &registry, sizeof(registry)) ||
            !m_process.ExpectBytes(keySlot, &roundKey, sizeof(roundKey)) ||
            !m_process.ExpectBytes(registry + 0x10, &roundMap, sizeof(roundMap)) ||
            !m_process.ExpectBytes(roundNode + 0x18, &round, sizeof(round))) throw std::runtime_error("Cannot guard retail round registry");
        if (!m_process.ExpectBytes(player + offsets.moveset, &oldRoot, sizeof(oldRoot)) ||
            !m_process.ExpectBytes(player + offsets.character, &targetCharacter, sizeof(targetCharacter)) ||
            !m_process.ExpectBytes(player + offsets.character + 4, &secondary, sizeof(secondary)) ||
            !m_process.ExpectBytes(root, live.data(), live.size())) throw std::runtime_error("Cannot guard retail import state");
        if (bindingPlayer && !m_process.ExpectBytes(bindingPlayer + offsets.moveset, &root, sizeof(root)))
            throw std::runtime_error("Cannot guard source moveset owner");
        // External cancel lists: from the live source root, or (memory-only native) from the file's capture, whose
        // pointers refer to the capture-time tables (the snapshot's own table addresses and original counts).
        const bool fileCancels = (m_memoryOnly || m_targetFallback) && m_native.has_value();
        auto tableBase = [&](size_t table) { return fileCancels ? entries[table].address : At<uint64_t>(live, R::Tables[table].pointerOffset); };
        auto tableCount = [&](size_t table) {
            if (!fileCancels) return At<uint64_t>(live, R::Tables[table].countOffset);
            return metadata ? At<uint64_t>(metadata->source, R::Tables[table].countOffset) : entries[table].count;
        };
        auto plan = RetailT8Import::Prepare(*outer, payload, [&](uint64_t row, uint32_t field) {
            const auto origin = metadata ? metadata->origins[12].at(row) : row;
            const std::vector<Byte>* captured = nullptr;
            if (fileCancels) {
                const auto found = m_native->cancels.find({static_cast<uint32_t>(origin), field});
                if (found != m_native->cancels.end()) captured = &found->second;
                else if (!m_targetFallback) throw std::runtime_error("External retail cancel list was not captured");
            }
            auto place = [&](size_t table, uint64_t id) { return entries[table].address + id * entries[table].stride; };
            if (m_targetFallback) {
                // The target's moveset does not have the export's lists: the capture (above) when the file has one, else
                // an empty list (a terminator: the move simply has no such cancels). Recorded for the image audit.
                std::vector<Byte> list;
                if (captured) {
                    auto sourceTable = [&](size_t table) { return std::pair<uint64_t, uint64_t>{tableBase(table), tableCount(table)}; };
                    for (size_t i = 0; (i + 1) * 40 <= captured->size(); ++i) {
                        auto rebased = RetailT8Import::RebaseExternalCancel(std::vector<Byte>(captured->begin() + i * 40,
                            captured->begin() + (i + 1) * 40), sourceTable, entries, metadata, place);
                        if (!rebased) { ++m_droppedExternalCancels; continue; }
                        const bool end = At<uint64_t>(*rebased, 0) == 0x8000;
                        list.insert(list.end(), rebased->begin(), rebased->end());
                        if (end) break;
                    }
                }
                if (list.size() < 40 || At<uint64_t>(list, list.size() - 40) != 0x8000) {
                    list = RetailT8Import::TerminatorCancel(payload, entries, place);
                    ++m_fallbackCancelLists;
                }
                m_fallbackCancels[{static_cast<uint32_t>(origin), field}] = list;
                return list;
            }
            const auto address = captured ? 0 : At<uint64_t>(moves, origin * 0x448 + field);
            auto sourceTable = [&](size_t table) { return std::pair<uint64_t, uint64_t>{tableBase(table), tableCount(table)}; };
            std::vector<Byte> list;
            for (size_t i = 0; i < 4096; ++i) {
                if (captured && (i + 1) * 40 > captured->size()) break;
                auto entry = captured ? std::vector<Byte>(captured->begin() + i * 40, captured->begin() + (i + 1) * 40)
                                      : Read(m_process, address + i * 40, 40);
                if (!captured && !m_process.ExpectBytes(address + i * 40, entry.data(), entry.size()))
                    throw std::runtime_error("Cannot guard external retail cancel list");
                auto rebased = RetailT8Import::RebaseExternalCancel(std::move(entry), sourceTable, entries, metadata, place);
                if (!rebased) { ++m_droppedExternalCancels; continue; } // it refers to something the edit deleted
                auto end = At<uint64_t>(*rebased, 0) == 0x8000;
                list.insert(list.end(), rebased->begin(), rebased->end());
                if (end) return list;
            }
            throw std::runtime_error("Unterminated external retail cancel list");
        });
        if (!plan.Ok()) throw std::runtime_error("Retail table pointer graph is invalid");
        if (convertedT7) memcpy(plan.tables.data() + 4, live.data() + 4, 4);
        const auto profile = ImportProfile::Read(*outer); // import profile: identity, gender, story flag
        const auto requirementRows = std::span<Byte>(plan.tables).subspan(plan.offsets[1], plan.counts[1] * R::Tables[1].stride);
        m_targetSecondary = secondary;
        m_characterFixes = RetailT8Import::AdaptCharacterRequirements(requirementRows,
            RetailT8Import::ProfileIdentity(profile, outer->characterId), targetCharacter, secondary);
        m_characterFixes += RetailT8Import::ApplyProfileConditions(requirementRows, profile);
        m_characterFixes += RetailT8Import::ApplyProfileProperties(
            std::span<Byte>(plan.tables).subspan(plan.offsets[9], plan.counts[9] * R::Tables[9].stride), profile);
        if (convertedT7) // the converter's placeholder identity becomes the target's (see ApplyConvertedIdentity)
            RetailT8Import::ApplyConvertedIdentity(std::span<Byte>(plan.tables).first(R::SourceHeaderSize),
                std::span<Byte>(plan.tables).subspan(plan.offsets[12], plan.counts[12] * 0x448), targetCharacter);
        if (!m_process.ExpectBytes(At<uint64_t>(live, 0x230), moves.data(), moves.size()))
            throw std::runtime_error("Cannot guard live move identities/resources");
        // Without memory-only registration the moves play the game's loaded animations: an animation the file carries
        // with other bytes (an edited one) cannot be uploaded on this route, so the game's own copy plays. Counted only.
        auto assets = R::Assets(payload);
        if (!assets.empty() && !m_memoryOnly && !m_targetFallback) { // the target fallback plays the target's own animations
            const auto container = m_game.ReadPtrPath("anim_container");
            for (const auto& asset : assets) {
                if (!asset.size || convertedT7 || asset.address == T7AnimToRetailT8::CustomAssetAddress) continue;
                auto panm = RT::Panm(m_process, container, animationCharacter, asset.instance, asset.type, asset.size);
                bool same = panm != 0;
                for (size_t at = 0; same && at < asset.size;) {
                    const auto length = (std::min)(size_t(1 << 20), static_cast<size_t>(asset.size - at));
                    std::vector<Byte> bytes(length);
                    same = m_process.probeBytes(panm + at, bytes.data(), length) && !memcmp(bytes.data(), payload.data() + asset.offset + at, length);
                    at += length;
                }
                if (!same) ++m_editedAnimationsKept;
            }
        }
        // Converted T7 animations: build each compiled animation locally, upload it and attach it to its carrier resource.
        // A read-only audit cannot run code in the game; it only counts them.
        m_customAnimations = 0; m_customAnimationsMissing = 0; m_restoredCarriers = 0; m_carrierFallbackMoves = 0; m_missingCarriers.clear();
        std::vector<gameAddr> attachedCarriers;
        // Memory-only: upload one entry per distinct PANM (or reuse our identical entry from an earlier import) and
        // link the new ones into the target's map. Everything is staged: nothing is visible before the root swap.
        std::map<uint32_t, uint64_t> memoryOnlyHandles; // file instance -> handle in the target's domain
        phase("prepare");
        if (m_memoryOnly) {
            namespace RS = RetailT8Resources;
            const auto engine = Engine();
            if (!m_process.ExpectBytes(targetMapSlot, &targetMap, sizeof(targetMap)))
                throw std::runtime_error("Cannot guard the target animation map");
            std::set<uint32_t> taken;
            std::map<std::pair<uint64_t, uint32_t>, std::vector<std::pair<std::span<const Byte>, uint32_t>>> uploaded;
            std::vector<RS::PublishedAnimation> publish;
            std::vector<std::pair<uint32_t, std::span<const Byte>>> uploads; // new entries: id, PANM
            {
                const auto fileMoves = R::Table(payload, 12);
                for (size_t row = 0; row < fileMoves.size() / 0x448; ++row)
                    if (const auto handle = At<uint64_t>(fileMoves, row * 0x448 + 0x50); handle >> 32) fromDisk(handle, row);
            }
            std::map<uint32_t, std::span<const Byte>> panms; // every animation the moves play: file, then archive
            for (const auto& [instance, asset] : bodyAssets) panms[instance] = payload.subspan(asset.offset, asset.size);
            for (const auto& [instance, bytes] : diskAssets) panms[instance] = bytes;
            m_archiveAnimations = diskAssets.size();
            // Id of the entry playing `panm`: one upload per distinct PANM, or our identical entry from an earlier import.
            auto chooseId = [&](std::span<const Byte> panm) {
                uint32_t digest = 0x811c9dc5; for (auto byte : panm) { digest ^= byte; digest *= 0x01000193; }
                auto& same = uploaded[{panm.size(), digest}];
                for (const auto& [bytes, existing] : same) if (std::equal(bytes.begin(), bytes.end(), panm.begin(), panm.end())) return existing;
                const auto chosen = RS::ChooseAnimationId(m_process, animContainer, m_memoryOnlyDomain, panm, engine, taken);
                taken.insert(chosen.id); same.push_back({panm, chosen.id});
                if (chosen.registered) ++m_reusedAnimations;
                else uploads.push_back({chosen.id, panm});
                return chosen.id;
            };
            for (const auto& [instance, panm] : panms) {
                // Native snapshot of an animation the target's maps already hold with the same bytes: bind to it.
                if (m_native) {
                    const auto loaded = RT::Panm(m_process, animContainer, m_memoryOnlyDomain, instance, 0, panm.size());
                    bool same = loaded != 0;
                    for (size_t at = 0; same && at < panm.size();) {
                        const auto length = (std::min)(size_t(1) << 20, panm.size() - at);
                        std::vector<Byte> bytes(length);
                        same = m_process.probeBytes(loaded + at, bytes.data(), length) && !memcmp(bytes.data(), panm.data() + at, length);
                        at += length;
                    }
                    // ...and only if the game would use that copy (its resolver skips a shared entry that is not ready).
                    same = same && RetailT8Import::ReadyAnimation(m_process, animContainer, m_memoryOnlyDomain, instance, engine.compiledVtable) != 0;
                    if (same) {
                        memoryOnlyHandles[instance] = 0xef00000000000000ULL | (uint64_t(m_memoryOnlyDomain & 0xff) << 40) | instance;
                        ++m_nativeBoundAnimations;
                        continue;
                    }
                }
                memoryOnlyHandles[instance] = 0xef00000000000000ULL | (uint64_t(m_memoryOnlyDomain & 0xff) << 40) | chooseId(panm);
            }
            // Converted T7 hand, face, wing and camera animations (categories 1..4, .docs/retail-t8-nonbody-anim-re.md): an
            // instance-map entry like a body animation, plus a node in the domain's category index. The file's index is
            // used when that key is free; a key holding another animation (the target's, or another import's) is never
            // shadowed: the next free index is taken and the rows are remapped to it (ApplyHandIndices below).
            std::vector<std::pair<uint32_t, uint64_t>> handNodes; // key, handle of the new category nodes
            gameAddr handMap = 0;
            if (convertedT7) {
                namespace H = T7HandToRetailT8;
                std::vector<R::AssetEntry> hands;
                for (const auto& asset : R::Assets(payload))
                    if ((asset.type == H::HandCategory || asset.type == T7CameraToRetailT8::CameraCategory ||
                         asset.type == T7WingToRetailT8::WingCategory || asset.type == T7FaceToRetailT8::FaceCategory) && asset.size &&
                        asset.address == T7AnimToRetailT8::CustomAssetAddress) hands.push_back(asset);
                m_handAnimations = hands.size();
                if (!hands.empty()) try { handMap = RS::CategoryIndexMap(m_process, m_game.ReadPtrPath("anim_manager"), m_memoryOnlyDomain); }
                catch (const std::exception& error) { DEBUG_ERR("Retail T8 import: no category index: %s", error.what()); handMap = 0; }
                std::set<uint32_t> usedKeys;
                constexpr uint32_t span = H::LastRetailIndex - H::FirstRetailIndex + 1;
                if (handMap) for (const auto& asset : hands) {
                    const auto fileIndex = H::AssetIndex(asset.instance);
                    if (fileIndex < H::FirstRetailIndex || fileIndex > H::LastRetailIndex || m_handIndices.count({asset.type, fileIndex})) continue;
                    const auto handle = 0xef00000000000000ULL | (uint64_t(m_memoryOnlyDomain & 0xff) << 40) | chooseId(payload.subspan(asset.offset, asset.size));
                    for (uint32_t probe = 0; probe < span; ++probe) {
                        const auto index = H::FirstRetailIndex + (fileIndex - H::FirstRetailIndex + probe) % span;
                        const auto key = RS::CategoryKey(asset.type, index);
                        if (usedKeys.count(key)) continue;
                        const auto existing = RS::CategoryHandle(m_process, handMap, key);
                        if (existing && existing != handle) continue;
                        usedKeys.insert(key); m_handIndices[{asset.type, fileIndex}] = index;
                        if (!existing) handNodes.push_back({key, handle}); // else: our own node from an earlier import
                        break;
                    }
                }
            }
            // One region for all new entries (16-aligned each; an entry's layout size does not depend on its address),
            // then the category nodes. Never freed: players keep pointers into it after binding (retained like published
            // movesets).
            std::vector<uint64_t> offsets; uint64_t total = 0;
            for (const auto& [id, panm] : uploads) {
                offsets.push_back(total);
                total = (total + RS::BuildRegisteredAnimation(panm, 0, engine, id).image.size() + 15) & ~uint64_t(15);
            }
            const auto nodesAt = total;
            total += handNodes.size() * 0x20;
            std::vector<RS::PublishedAnimation> handPublish;
            if (total) {
                const auto region = m_process.allocateMem(total);
                if (!region) return ImportationErrcode_GameAllocationErr;
                for (size_t i = 0; i < uploads.size(); ++i) {
                    const auto entry = RS::BuildRegisteredAnimation(uploads[i].second, region + offsets[i], engine, uploads[i].first);
                    if (offsets[i] + entry.image.size() > nodesAt || !m_process.writeBytes(region + offsets[i], entry.image.data(), entry.image.size()))
                        return ImportationErrcode_GameWriteError;
                    publish.push_back({uploads[i].first, entry.node});
                }
                for (size_t i = 0; i < handNodes.size(); ++i) {
                    const auto node = RS::CategoryNode(handNodes[i].first, handNodes[i].second);
                    if (!m_process.writeBytes(region + nodesAt + i * 0x20, node.data(), node.size())) return ImportationErrcode_GameWriteError;
                    handPublish.push_back({handNodes[i].first, region + nodesAt + i * 0x20});
                }
            }
            m_publishedAnimations = RS::PublishAnimations(m_process, targetMap, publish);
            // After the entries: a published index resolves to an entry that is already linked (both in this batch).
            m_publishedHandIndices = RS::PublishCategoryIndex(m_process, handMap, handPublish);
            m_customAnimations = panms.size();
        }
        // Hand rows play the published index; without one (another route, no category index, no free key) they never fire.
        if (convertedT7) {
            const auto rows = std::span<Byte>(plan.tables).subspan(plan.offsets[9], plan.counts[9] * R::Tables[9].stride);
            RetailT8Import::ApplyHandIndices(rows, m_memoryOnly ? &m_handIndices : nullptr, &m_silencedHandProperties);
        }
        for (const auto& asset : assets) {
            if (m_memoryOnly || m_targetFallback) break; // the target fallback writes nothing into game resources
            if (asset.address != T7AnimToRetailT8::CustomAssetAddress || !asset.size) continue;
            if (m_process.CanPrepareGameResources()) {
                const auto container = m_game.ReadPtrPath("anim_container");
                const auto resource = RetailT8Resources::FindAnimationResource(m_process, container, animationCharacter, asset.instance);
                // No carrier for it in the game: its moves play the idle animation (FallBackMissingCarriers below).
                if (!resource) { m_missingCarriers.insert(asset.instance); ++m_customAnimationsMissing; continue; }
                // Upload and +0x190 store go through m_process: staged in this import's batch, undone if it fails.
                RetailT8Resources::AttachCustomAnimation(m_process, Engine(), resource, payload.subspan(asset.offset, asset.size), player);
                attachedCarriers.push_back(resource);
            }
            ++m_customAnimations;
        }
        m_carrierFallbackMoves = RetailT8Import::FallBackMissingCarriers(animationRows, payload, m_missingCarriers);
        // Rebind all runtime strings/resources to the live bound root; never trust the
        // export's original addresses. Body handles refer to game-owned PANMs.
        for (const auto& dependency : plan.dependencies) {
            uint64_t value;
            if (dependency.field < R::SourceHeaderSize) value = At<uint64_t>(live, dependency.field);
            else {
                auto field = dependency.field - plan.offsets[12];
                const auto row=field/0x448, member=field%0x448;
                auto origin=metadata?metadata->origins[12].at(row):row;
                field=((member==0x40 && !convertedT7 && origin<moves.size()/0x448)?origin:animationRows[row])*0x448+member;
                value = At<uint64_t>(moves, field);
            }
            Put(plan.tables, dependency.field, value);
        }
        if (m_memoryOnly) {
            // Body handles: our entries; a move whose animation the file does not carry plays the idle animation.
            const auto fileMoves = R::Table(payload, 12);
            const auto domainHandle = 0xef00000000000000ULL | (uint64_t(m_memoryOnlyDomain & 0xff) << 40);
            // Every move gets its own animation (file or archive, bound or uploaded above), else the target's idle.
            for (size_t row = 0; row < fileMoves.size() / 0x448; ++row) {
                const auto handle = At<uint64_t>(fileMoves, row * 0x448 + 0x50);
                const auto field = plan.offsets[12] + row * 0x448 + 0x50;
                if (handle >> 32) {
                    const auto found = memoryOnlyHandles.find(static_cast<uint32_t>(handle));
                    if (found != memoryOnlyHandles.end()) Put(plan.tables, field, found->second);
                    // No data exists in the game for it: the move points at the same id, as the game's own moveset does.
                    else if (m_dataless.count(static_cast<uint32_t>(handle))) Put(plan.tables, field, domainHandle | static_cast<uint32_t>(handle));
                    // Found nowhere: the target's idle animation.
                    else { Put(plan.tables, field, At<uint64_t>(targetMoves, m_memoryOnlyStringRow * 0x448 + 0x50)); ++m_memoryOnlyFallbackMoves; }
                }
                m_memoryOnlyHandles.push_back(At<uint64_t>(std::span<const Byte>(plan.tables), field));
            }
            // Native snapshots: their own move strings, stored in the allocation (unknown ones keep the target row's).
            if (m_native) {
                std::map<std::string, uint64_t> pooled;
                for (size_t row = 0; row < fileMoves.size() / 0x448; ++row) {
                    const auto origin = metadata ? metadata->origins[12].at(row) : row;
                    std::optional<std::string> texts[2];
                    if (origin < m_native->names.size()) { texts[0] = m_native->names[origin]; texts[1] = m_native->animations[origin]; }
                    for (size_t i = 0; i < 2; ++i) {
                        if (!texts[i]) continue;
                        auto [it, inserted] = pooled.try_emplace(*texts[i], plan.tables.size());
                        if (inserted) plan.tables.insert(plan.tables.end(), texts[i]->c_str(), texts[i]->c_str() + texts[i]->size() + 1);
                        const auto field = plan.offsets[12] + row * 0x448 + (i ? 0x48 : 0x40);
                        Put(plan.tables, field, 0); plan.relocations.push_back({field, it->second});
                    }
                    m_memoryOnlyNames.push_back(texts[0]); m_memoryOnlyAnimations.push_back(texts[1]);
                }
            }
            m_memoryOnlyContainer = animContainer;
        }
        // R::Table validates the whole snapshot: once, not per row.
        const auto metadataMoves=metadata?R::Table(payload,12):std::span<const Byte>();
        if(metadata)for(size_t row=0;row<entries[12].count;++row){
            auto record=metadataMoves.subspan(row*0x448,0x448);auto hash=EditorT8Retail::Decode(At<uint64_t>(record,0),At<uint64_t>(record,8));
            if(hash && metadata->names.contains(*hash)){
                const auto& name=metadata->names.at(*hash);const auto offset=plan.tables.size();plan.tables.insert(plan.tables.end(),name.c_str(),name.c_str()+name.size()+1);
                const auto field=plan.offsets[12]+row*0x448+0x40;Put(plan.tables,field,0);plan.relocations.push_back({field,offset});
            }
        }
        // Carriers this player used for an earlier document's custom PANMs get their native animation back.
        if (m_process.CanPrepareGameResources())
            m_restoredCarriers = RetailT8Resources::RestoreCarriers(m_process, player, attachedCarriers);
        progress = 60;
        phase("animations");
        // Stock resources remain owned by the loaded round. Published allocations
        // are retained until process exit; no timer-based free is safe here.
        const auto allocation = m_process.allocateMem(plan.tables.size());
        if (!allocation) return ImportationErrcode_GameAllocationErr;
        auto bytes = plan.RelocateTables(allocation);
        if (!m_process.writeBytes(allocation, bytes.data(), bytes.size())) return ImportationErrcode_GameWriteError;
        phase("moveset");
        if (settings & ImportSettings_StageOnly) {
            // Everything is written except the player's pointers: the allocation stays unreferenced (retained).
            lastLoaded.address = allocation; lastLoaded.size = bytes.size(); lastLoaded.crc32 = outer->crc32;
            lastLoaded.charId = static_cast<uint8_t>(outer->characterId);
            progress = 100; return ImportationErrcode_Successful;
        }
        if (RT::Read<uint64_t>(m_process, player + offsets.moveset) != oldRoot ||
            RT::Read<uint32_t>(m_process, player + offsets.character) != targetCharacter ||
            (bindingPlayer && RT::Read<uint64_t>(m_process, bindingPlayer + offsets.moveset) != root))
            throw std::runtime_error("Retail round changed during import");
        if (!m_process.writeUInt64(player + offsets.moveset, allocation)) return ImportationErrcode_GameWriteError;
        // Current move/context remain valid while the game transitions naturally.
        // For instant application use the supported next-move queue, with the
        // corresponding context roots, rather than replacing cached move pointers.
        if (settings & ImportSettings_ApplyInstantly) {
            const auto idle = At<uint16_t>(bytes, 0xaa); // GetMoveFromID: aliases at +0xa8
            if (idle >= entries[12].count) throw std::runtime_error("Retail idle alias is out of range");
            const auto currentMove = RT::Read<uint64_t>(m_process, player + offsets.currentMove);
            const auto length = RT::Read<uint32_t>(m_process, currentMove + 0x120);
            const auto contexts = Read(m_process, player + offsets.moveset + 8, 32);
            if (!m_process.ExpectBytes(player + offsets.currentMove, &currentMove, sizeof(currentMove)) ||
                !m_process.ExpectBytes(player + offsets.moveset + 8, contexts.data(), contexts.size()))
                throw std::runtime_error("Cannot guard retail move/context transition");
            for (size_t i = 1; i < 5; ++i) if (!m_process.writeUInt64(player + offsets.moveset + i * 8, allocation)) return ImportationErrcode_GameWriteError;
            if (!m_process.writeUInt32(player + offsets.nextMoveId, 0x8001) ||
                !m_process.writeUInt64(player + offsets.nextMove, allocation + plan.offsets[12] + idle * 0x448) ||
                !m_process.writeUInt32(player + offsets.currentMoveTimer, length)) return ImportationErrcode_GameWriteError;
        }
        lastLoaded.address = allocation; lastLoaded.size = bytes.size(); lastLoaded.crc32 = outer->crc32;
        lastLoaded.charId = static_cast<uint8_t>(outer->characterId);
        phase("publish");
        progress = 100; return ImportationErrcode_Successful;
    } catch (const std::exception& error) {
        m_error = error.what(); DEBUG_ERR("Retail T8 import failed: %s", error.what());
        return ImportationErrcode_MovesetDataError;
    }
}
