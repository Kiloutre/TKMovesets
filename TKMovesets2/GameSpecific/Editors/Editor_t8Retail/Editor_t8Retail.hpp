#pragma once
#include "EditorLogic.hpp"
#include "AnimationPreview/PreviewDisplacement.hpp"
#include "AnimationPreview/PreviewHitbox.hpp"
#include "Extractor_t8/RetailSnapshot.hpp"
#include <array>
#include <filesystem>
#include <functional>
#include <optional>
#include "RetailEditorData.hpp"

// Retail T8RS records retain source addresses. Editing never uses CNT structs.
class EditorT8Retail final : public EditorLogic {
public:
    enum class Kind { U16, S16, U32, U64, S32, F32, Reference, Encoded32 };
    struct Field { std::string name; uint32_t offset; Kind kind; int target = -1; bool readOnly = false; };
    using EditorLogic::EditorLogic;
    static EditorWindowType Type(size_t table);
    static size_t TableIndex(EditorWindowType type);
    static const std::vector<Field>& Schema(size_t table);
    static std::optional<uint32_t> Decode(uint64_t value, uint64_t key);
    static uint64_t Encode(uint32_t value, uint64_t key);
    // Plain copy of an encrypted move field (block {value, key, four u32 slots} at `field` in move `row`): rows with
    // row % 8 >= 4 keep the plain value in slot row % 8 - 4; other rows have no copy and are left unchanged.
    static void SetPlainCopy(Byte* field, uint32_t row, uint32_t value);
    uint64_t Count(size_t table) const;
    uint32_t MoveFingerprint(uint32_t row) const;
    bool CanDeleteRecord(size_t table,uint32_t row) const;
    uint32_t Character() const { return m_header->characterId; }
    std::span<const Byte> Record(size_t table, uint32_t row) const;
    std::optional<uint32_t> MoveNameHash(uint32_t row) const;
    uint32_t ResolveMove(uint32_t id) const;
    std::vector<uint32_t> MoveAliases(uint32_t row) const;
    // T7-style summary of an input sequence: decoded inputs ("D, D/F, F+1") or "N inputs" when long/empty.
    std::string InputSequenceText(uint32_t sequence) const;
    bool MatchesMoveNames(std::span<const Byte> loaded) const;
    struct Reference {size_t table;uint32_t row;std::string field;};
    std::vector<Reference> ReferencesTo(size_t table,uint32_t row) const;
    // Row a Reference-kind field of (table,row) points at; none when the pointer is null or external.
    std::optional<uint32_t> ReferenceIndex(size_t table,uint32_t row,const Field& field) const;
    // Frame count of a native (raw or compressed) PANM; throws on an invalid structure. RetailPanmOperations.cpp.
    static uint32_t PanmFrames(std::span<const Byte> panm);
    InputMap RecordFields(size_t table, uint32_t row, VectorSet<std::string>& order);
    void SaveRecord(size_t table, uint32_t row, InputMap& fields);
    size_t ListSize(size_t table, uint32_t row) const;
    void RenameMove(uint32_t row, std::string_view name);
    void SetAnimation(uint32_t row, uint32_t donor);
    uint32_t FindAnimationFile(const wchar_t* path) const;
    // Standalone PANM export/import (body animations, asset category 0). RetailPanmOperations.cpp.
    // custom: stored as a custom asset (else bound to identical embedded bytes); replaced: it took the place of the
    // move's own custom animation (used by no other move) instead of a new instance; name: its animation name.
    struct PanmImport { uint32_t row = 0; uint32_t instance = 0; bool custom = false; bool replaced = false; bool rewritten = false; std::string name; };
    static std::vector<Byte> ReadPanmFile(const std::filesystem::path& path);
    // An animation file for import: a retail PANM as is, or a Tekken 7 0x64 / 0xC8 animation (either byte order)
    // converted onto the retail skeleton (T7AnimToRetailT8, as T7 movesets are converted).
    static std::vector<Byte> ReadAnimationFile(const std::filesystem::path& path);
    // Throws unless the PANM is a native body animation (Top/Trans, >= 20 bones); raw files must also match the native template layout.
    static void ValidatePanm(std::span<const Byte> panm);
    // Raw PANM from another tool written over a native body animation embedded in this document
    // (T7AnimToRetailT8::Rewrite); ImportPanm does this automatically when ValidatePanm refuses a file.
    std::vector<Byte> RewritePanm(std::span<const Byte> panm) const;
    std::optional<uint32_t> AnimationInstance(uint32_t row) const;
    std::vector<Byte> MoveAnimation(uint32_t row) const;
    // The move's extra properties that the animation preview applies (AnimPreview::T8DisplacementKind), as saved.
    // Properties with conditions (a requirement other than 0, which is always true) are counted in `conditional`, not
    // returned.
    std::vector<AnimPreview::Displacement> MoveDisplacements(uint32_t row, int* conditional = nullptr) const;
    // The move's used hitbox slots (location != 0), as saved, for the animation preview.
    std::vector<AnimPreview::HitboxSlot> MoveHitboxes(uint32_t row) const;
    // Reaction moves (rows, aliases resolved) of the move's hit on the animation preview's opponent: the first hit
    // condition whose requirements hold for an opponent airborne or not (only "opponent airborne / not airborne / in a
    // juggle" are judged, other requirements count as unmet), down to the list's default entry. All -1 when the move
    // has no hit condition.
    AnimPreview::ReactionMoves MoveHitReactions(uint32_t row, bool opponentAirborne) const;
    // The move's hit uses the tornado: one of its hit conditions requires 724 "Tornado Available".
    bool MoveUsesTornado(uint32_t row) const;
    // A move already bound to these exact bytes (native or custom asset).
    std::optional<uint32_t> FindBoundAnimation(std::span<const Byte> panm) const;
    // Writes every distinct embedded body animation used by a move plus animations.json.
    size_t ExportAnimations(const std::filesystem::path& newDirectory, const std::function<std::string(uint32_t)>& name = {}) const;
    // Byte-identical PANMs reuse the existing binding. Others become a custom asset under a new instance and a new
    // animation name (`name`, made unique) for this move only; no other move's animation changes. A custom animation
    // only this move uses is replaced in place. The memory-only import registers it under a free game id.
    PanmImport ImportPanm(uint32_t row, std::span<const Byte> panm, std::string_view name = {});
    // Converted documents from older versions hold placeholder body handles; copy the native handle of the donor
    // move with the same animation hash. Returns the number of moves updated.
    size_t AdoptAnimationHandles(std::span<const Byte> donorMoves);
    const std::string* CustomName(uint32_t hash) const;
    // Name from the file's names section (RetailT8Snapshot::NamesTag), nullptr when it has none for `hash`.
    const std::string* FileName(uint32_t hash) const;
    void EditRows(size_t table, uint32_t first, const std::vector<int>& ids, const std::set<int>& deleted);
    bool LoadMoveset(Byte*, uint64_t) override;
    void LoadMovesetPtr(Byte*, uint64_t) override;
    InputMap GetFormFields(EditorWindowType type, uint16_t id, VectorSet<std::string>& order) override { return RecordFields(TableIndex(type), id, order); }
    std::vector<InputMap> GetFormFieldsList(EditorWindowType, uint16_t, VectorSet<std::string>&) override;
    std::vector<InputMap> GetFormFieldsList(EditorWindowType, uint16_t, VectorSet<std::string>&, int) override;
    InputMap GetListSingleForm(EditorWindowType type, uint16_t id, VectorSet<std::string>& order) override { return GetFormFields(type, id, order); }
    InputMap GetListSingleFormWide(EditorWindowType type,uint32_t id,VectorSet<std::string>& order) override {return RecordFields(TableIndex(type),id,order);}
    std::vector<InputMap> GetFormFieldsListWide(EditorWindowType,uint32_t,VectorSet<std::string>&,int) override;
    void SaveItemWide(EditorWindowType type,uint32_t id,InputMap& fields) override {SaveRecord(TableIndex(type),id,fields);}
    bool ValidateField(EditorWindowType, EditorInput*) override;
    void SaveItem(EditorWindowType type, uint16_t id, InputMap& fields) override { SaveRecord(TableIndex(type), id, fields); }
    std::string ImportAnimation(const wchar_t*, int) override;
    uint32_t CreateNew(EditorWindowType) override;
    void ModifyListSize(EditorWindowType, unsigned int, const std::vector<int>&, const std::set<int>&) override;
    uint32_t DuplicateStructure(EditorWindowType, uint32_t, size_t) override;
    void DeleteStructures(EditorWindowType, uint32_t, size_t) override;
    // No process access even if a caller accidentally supplies a nonzero live binding.
    void Live_OnFieldEdit(EditorWindowType, int, EditorInput*) override { live_loadedMoveset = 0; }
    uint32_t CalculateCRC32() override;
protected:
    void RestoreMovesetPtr(Byte*,uint64_t) noexcept override;
    std::unique_ptr<MutationState> CaptureMutationState() override;
private:
    std::shared_ptr<const RetailEditorData::Metadata> m_metadata;
    void Publish(std::span<const Byte> payload, const RetailEditorData::Metadata& metadata);
    void Rebuild(size_t table, const RecordEdit& edit, const std::vector<Byte>& replacement, const std::vector<uint32_t>& origins);
    std::array<RetailT8Snapshot::Entry, 20> m_entries{};
    // (table, row) of the zero-filled rows the last EditRows inserted; SaveRecord does not treat them as terminators.
    std::set<std::pair<size_t, uint32_t>> m_insertedRows;
    static std::vector<Field> BuildSchema(size_t table);
    std::string FieldText(const Field&,EditorInput*) const;
    uint64_t Parse(const Field&, std::string_view, std::span<const Byte>) const;
};
