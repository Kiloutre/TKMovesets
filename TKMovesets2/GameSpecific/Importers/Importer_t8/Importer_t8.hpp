#pragma once
#include "Importer.hpp"
#include "Extractor_t8/RetailSnapshot.hpp"
#include "Extractor_t8/RetailRuntime.hpp"
#include "T7ToRetailT8.hpp"
#include "RetailResourceLoader.hpp"
#include "GameStaticData.hpp"
#include <map>
#include <optional>
#include <string>
#include <set>
#include <vector>

class ImporterT8 : public Importer {
    std::string m_error;
    gameAddr m_bindingRoot = 0;
    uint64_t m_characterFixes = 0;
    uint32_t m_targetSecondary = UINT32_MAX; // target's secondary identity (+0x16c) at the last import
    uint64_t m_customAnimations = 0, m_customAnimationsMissing = 0, m_restoredCarriers = 0, m_carrierFallbackMoves = 0;
    std::set<uint32_t> m_missingCarriers;
    bool m_memoryOnly = false;
    // No loaded moveset could lend the file what it lacks: the target's own moveset did (see TargetFallback()).
    bool m_targetFallback = false;
    // External cancel lists written on the target fallback, keyed by captured move row and field (final bytes).
    std::map<std::pair<uint32_t, uint32_t>, std::vector<Byte>> m_fallbackCancels;
    uint64_t m_fallbackCancelLists = 0;
    uint64_t m_publishedAnimations = 0, m_reusedAnimations = 0, m_memoryOnlyFallbackMoves = 0, m_memoryOnlyStringRow = 0;
    std::vector<std::pair<std::string, double>> m_timings;
    uint32_t m_memoryOnlyDomain = 0;
    gameAddr m_memoryOnlyContainer = 0;
    std::vector<uint64_t> m_memoryOnlyHandles;
    uint64_t m_nativeBoundAnimations = 0, m_nativeUnloadedKept = 0, m_archiveAnimations = 0;
    std::set<uint32_t> m_dataless, m_unavailableAnimations;
    std::string m_archiveError;
    uint64_t m_bindingIdentityMismatches = 0, m_bindingFallbackMoves = 0, m_droppedExternalCancels = 0, m_editedAnimationsKept = 0;
    // animation character -> archive code. Shared, parsed once: an importer is built per preview refresh.
    const std::shared_ptr<const GameStaticData> m_names = GameStaticData::LoadShared("t8-retail");
    std::optional<RetailT8Snapshot::NativeData> m_native;
    std::vector<std::optional<std::string>> m_memoryOnlyNames, m_memoryOnlyAnimations;
    ImportationErrcode_ ImportMovesetData(const TKMovesetHeader*, Byte*, uint64_t, gameAddr, ImportSettings, uint8_t&) override;
public:
    using Importer::Importer;
    const std::string& LastError() const override { return m_error; }
    gameAddr BindingRoot() const { return m_bindingRoot; }
    // Target fallback: no loaded moveset matched the file (or its character's animations were not loaded), so the
    // target's own moveset provided strings and animations. Moves play the target's animation of the same hash when it
    // has one, else its idle (BindingFallbackMoves). External cancel lists the target cannot provide come from the
    // file's capture when present, else become an empty list (FallbackCancelLists).
    bool TargetFallback() const { return m_targetFallback; }
    const std::map<std::pair<uint32_t, uint32_t>, std::vector<Byte>>& FallbackCancels() const { return m_fallbackCancels; }
    uint64_t FallbackCancelLists() const { return m_fallbackCancelLists; }
    RetailT8Runtime::PlayerOffsets Offsets() const {
        auto value = [this](const char* key) { return static_cast<uint64_t>(m_game.GetValue(key)); };
        return { value("motbin_offset"), value("chara_id_offset"), value("currmove"), value("currmove_timer"),
            value("nextmove_addr"), value("nextmove_id") };
    }
    // Game objects our animation entries point at (game_addresses.ini t8-retail_anim_*_rva). Throws when missing.
    RetailT8Resources::EngineAddresses Engine() const { return RetailT8Resources::ReadEngineAddresses(m_game, m_process.mainModule.address); }
    uint64_t CharacterFixes() const { return m_characterFixes; }
    uint32_t TargetSecondary() const { return m_targetSecondary; }
    // Converted T7 animations compiled and attached to carrier resources (counted only in read-only audits).
    uint64_t CustomAnimations() const { return m_customAnimations; }
    uint64_t CustomAnimationsMissing() const { return m_customAnimationsMissing; }
    // Carrier instances absent from the game; their converted moves were bound to the idle animation.
    const std::set<uint32_t>& MissingCarriers() const { return m_missingCarriers; }
    uint64_t CarrierFallbackMoves() const { return m_carrierFallbackMoves; }
    // Carriers from an earlier import on this player given back their native animation.
    uint64_t RestoredCarriers() const { return m_restoredCarriers; }
    // Memory-only import (converted T7 with embedded PANMs): entries registered in the target's own animation map.
    bool MemoryOnly() const { return m_memoryOnly; }
    // Native moves on their own character whose animation was not loaded at export: original handle kept.
    uint64_t NativeUnloadedKept() const { return m_nativeUnloadedKept; }
    // Animations the file did not carry, imported from the game's tkdata.bin.
    uint64_t ArchiveAnimations() const { return m_archiveAnimations; }
    // Animation ids the game's archives declare without data (nothing to import; the move keeps the id).
    const std::set<uint32_t>& DatalessAnimations() const { return m_dataless; }
    uint64_t PublishedAnimations() const { return m_publishedAnimations; }
    uint64_t ReusedAnimations() const { return m_reusedAnimations; }
    // Moves whose animation the file does not carry, bound to the idle animation.
    uint64_t MemoryOnlyFallbackMoves() const { return m_memoryOnlyFallbackMoves; }
    // Animations neither the file nor tkdata.bin could provide (their moves play the idle animation).
    const std::set<uint32_t>& UnavailableAnimations() const { return m_unavailableAnimations; }
    // First tkdata.bin error behind UnavailableAnimations (empty if none): may be environmental (missing tkdata.bin,
    // no archive code because TKMInterface/t8-retail_names.json is missing), not only an absent animation.
    const std::string& ArchiveError() const { return m_archiveError; }
    // Binding route (native files that cannot go memory-only): rows of the chosen loaded moveset whose identity differs
    // from the export, and moves bound to its idle animation because it does not play theirs.
    uint64_t BindingIdentityMismatches() const { return m_bindingIdentityMismatches; }
    uint64_t BindingFallbackMoves() const { return m_bindingFallbackMoves; }
    // External cancels dropped because they referred to something the edit deleted.
    uint64_t DroppedExternalCancels() const { return m_droppedExternalCancels; }
    // Binding route: animations the file carries with other bytes than the game's; the game's copy plays.
    uint64_t EditedAnimationsKept() const { return m_editedAnimationsKept; }
    // Milliseconds per import phase of the last import, in order (prepare, animations, moveset, publish).
    const std::vector<std::pair<std::string, double>>& Timings() const { return m_timings; }
    // T7 file -> retail T8 file (self-contained conversion; `player` is unused, the output is the same for every target).
    std::vector<Byte> ConvertT7(const TKMovesetHeader& outer, const StructsT7::TKMovesetHeaderBlocks& blocks,
        std::span<const Byte> t7Payload, gameAddr player);
    // Audit inputs: expected body handle per move, the target row lending move strings, the domain and container.
    const std::vector<uint64_t>& MemoryOnlyHandles() const { return m_memoryOnlyHandles; }
    uint64_t MemoryOnlyStringRow() const { return m_memoryOnlyStringRow; }
    uint32_t MemoryOnlyDomain() const { return m_memoryOnlyDomain; }
    gameAddr MemoryOnlyContainer() const { return m_memoryOnlyContainer; }
    // v4 native snapshots: animations bound to the target's own identical resident copy (no upload), the
    // captured data used, and the localised move strings per row (nullopt: the target row's string).
    uint64_t NativeBoundAnimations() const { return m_nativeBoundAnimations; }
    const std::optional<RetailT8Snapshot::NativeData>& Native() const { return m_native; }
    const std::vector<std::optional<std::string>>& MemoryOnlyNames() const { return m_memoryOnlyNames; }
    const std::vector<std::optional<std::string>>& MemoryOnlyAnimations() const { return m_memoryOnlyAnimations; }
    bool CanImport() override;
    gameAddr GetCharacterAddress(uint8_t player) override;
    gameAddr GetMovesetAddress(uint8_t player) override;
    // Game/animation caches may retain references after a move/round changes.
    // Keep published allocations until process exit rather than guessing liveness.
    void CleanupUnusedMovesets() override {}
};
