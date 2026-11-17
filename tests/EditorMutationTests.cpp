#include "Editor_t7/Editor_t7.hpp"
#include "Importer_t7/Importer_t7_Utils.hpp"
#include <cstring>
#include <stdexcept>
#include <vector>

namespace {
void Check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
class Editor : public EditorT7
{
public:
    using EditorT7::EditorT7;
    bool failAllocation = false;
protected:
    Byte* AllocateMutationBuffer(size_t size) override {
        return failAllocation ? nullptr : EditorT7::AllocateMutationBuffer(size);
    }
};
std::vector<Byte> Snapshot(Editor& editor)
{
    uint64_t size;
    const auto bytes = editor.GetMoveset(size);
    return {bytes, bytes + size};
}
void Load(Editor& editor, bool training = false)
{
    using namespace StructsT7;
    TKMovesetHeader header;
    header.header_size = header.block_list = sizeof(header);
    header.block_list_size = 8;
    header.moveset_data_start = sizeof(header) + sizeof(TKMovesetHeaderBlocks);
    TKMovesetHeaderBlocks blocks{};
    blocks.tableBlock = offsetof(MovesetInfo, table);
    blocks.motalistsBlock = offsetof(MovesetInfo, motas);
    blocks.nameBlock = blocks.movesetBlock = sizeof(MovesetInfo);
    const auto sequenceOffset = 4 * sizeof(Requirement) + 2 * sizeof(Cancel);
    const auto inputOffset = sequenceOffset + sizeof(InputSequence);
    const auto structureSize = inputOffset + 3 * sizeof(Input);
    blocks.animationBlock = blocks.movesetBlock + structureSize;
    blocks.motaBlock = blocks.animationBlock + 8;
    blocks.movelistBlock = blocks.motaBlock + 8;
    const size_t displayables = 40;
    const size_t playables = displayables + 2 * sizeof(MvlDisplayable);
    const size_t inputs = playables + 2 * sizeof(MvlPlayable);
    auto size = header.moveset_data_start + blocks.movelistBlock + (training ? inputs + 3 * sizeof(MvlInput) : 0);
    auto bytes = static_cast<Byte*>(calloc(1, size));
    if (!bytes) throw std::bad_alloc();
    memcpy(bytes, &header, sizeof(header));
    memcpy(bytes + header.block_list, &blocks, sizeof(blocks));
    auto info = reinterpret_cast<StructsT7_gameAddr::MovesetInfo*>(bytes + header.moveset_data_start);
    memset(&info->motas, 0xff, sizeof(info->motas));
    for (auto& entry : info->table.entries) entry.listAddr = structureSize;
    info->table.requirement = 0;
    info->table.requirementCount = 4;
    info->table.cancel = 4 * sizeof(Requirement);
    info->table.cancelCount = 2;
    info->table.inputSequence = sequenceOffset;
    info->table.inputSequenceCount = 1;
    info->table.input = inputOffset;
    info->table.inputCount = 3;
    auto data = bytes + header.moveset_data_start + blocks.movesetBlock;
    auto requirements = reinterpret_cast<Requirement*>(data);
    for (int i = 0; i < 4; ++i) requirements[i].condition = 100 + i;
    auto sequence = reinterpret_cast<StructsT7_gameAddr::InputSequence*>(data + sequenceOffset);
    sequence->input_addr = 0;
    sequence->input_amount = 3;
    auto cancels = reinterpret_cast<StructsT7_gameAddr::Cancel*>(data + info->table.cancel);
    for (int i = 0; i < 2; ++i) {
        cancels[i].requirements_addr = 3;
        cancels[i].extradata_addr = UINT64_MAX;
    }
    memcpy(bytes + header.moveset_data_start + blocks.motaBlock, "MOTAdata", 8);
    if (training) {
        auto mvlBytes = bytes + header.moveset_data_start + blocks.movelistBlock;
        auto mvl = reinterpret_cast<MvlHead*>(mvlBytes);
        memcpy(mvl->mvlString, "MVLT", 4);
        mvl->displayables_count = mvl->playables_count = 2;
        mvl->displayables_offset = displayables;
        mvl->playables_offset = playables;
        mvl->inputs_offset = inputs;
        memcpy(mvlBytes + 32, "shared", 7);
        auto rows = reinterpret_cast<MvlDisplayable*>(mvlBytes + displayables);
        for (int i = 0; i < 2; ++i) {
            for (auto& text : rows[i].all_translation_offsets) text = 32;
            rows[i].playable_id = 1;
        }
        auto sequences = reinterpret_cast<MvlPlayable*>(mvlBytes + playables);
        sequences[0].input_sequence_offset = 0;
        sequences[0].input_count = 2;
        sequences[1].input_sequence_offset = 2;
        sequences[1].input_count = 1;
        auto events = reinterpret_cast<MvlInput*>(mvlBytes + inputs);
        for (int i = 0; i < 3; ++i) events[i].frame_duration = 10 * (i + 1);
        editor.hasDisplayableMovelist = true;
    }
    editor.LoadMovesetPtr(bytes, size);
}
const StructsT7_gameAddr::MovesetInfo& Info(const std::vector<Byte>& bytes)
{
    auto header = reinterpret_cast<const TKMovesetHeader*>(bytes.data());
    return *reinterpret_cast<const StructsT7_gameAddr::MovesetInfo*>(bytes.data() + header->moveset_data_start);
}
void Verify(Editor& editor, std::initializer_list<uint32_t> conditions, uint64_t reference)
{
    auto bytes = Snapshot(editor);
    auto header = reinterpret_cast<const TKMovesetHeader*>(bytes.data());
    auto blocks = reinterpret_cast<const StructsT7::TKMovesetHeaderBlocks*>(bytes.data() + header->block_list);
    auto& info = Info(bytes);
    auto payload = bytes.data() + header->moveset_data_start;
    Check(T7ImportUtils::ValidateT7Layout(*blocks, payload, bytes.size() - header->moveset_data_start), "edited layout invalid");
    Check(info.table.requirementCount == conditions.size(), "wrong edited table count");
    auto requirements = reinterpret_cast<const StructsT7::Requirement*>(payload + blocks->movesetBlock + info.table.requirement);
    size_t i = 0;
    for (auto value : conditions) Check(requirements[i++].condition == value, "record copy used the wrong source");
    auto cancels = reinterpret_cast<const StructsT7_gameAddr::Cancel*>(payload + blocks->movesetBlock + info.table.cancel);
    Check(cancels[0].requirements_addr == reference, "reference mapping lost record identity");
    Check(memcmp(bytes.data() + bytes.size() - 8, "MOTAdata", 8) == 0, "following block was damaged");
}
template<class Operation> void Rejected(Editor& editor, Operation operation)
{
    auto before = Snapshot(editor);
    uint64_t size;
    auto pointer = editor.GetMoveset(size);
    editor.live_loadedMoveset = 0x1234;
    bool failed = false;
    try { operation(); } catch (const std::exception&) { failed = true; }
    Check(failed, "invalid edit was accepted");
    Check(Snapshot(editor) == before && editor.GetMoveset(size) == pointer, "failed edit changed the original allocation");
    Check(editor.live_loadedMoveset == 0x1234, "failed edit lost the live binding");
}
}

void RunEditorMutationTests()
{
    GameProcess process;
    GameData game;
    GameInfo info{};
    info.dataString = info.minorDataString = "t7";
    // Create new: a moveset without a displayable movelist (converted from T8) refuses movelist rows, unchanged,
    // and a new input list is one input, the list size the toolbar opens it with.
    {
        Editor editor(process, game, &info); Load(editor);
        Check(!editor.hasDisplayableMovelist, "fixture without a movelist has one");
        Rejected(editor, [&] { editor.CreateNew(TEditorWindowType_MovelistPlayable); });
        Rejected(editor, [&] { editor.CreateNew(TEditorWindowType_MovelistInput); });
        const auto input = editor.CreateNew(TEditorWindowType_Input);
        VectorSet<std::string> drawOrder;
        Check(input == 3 && editor.GetFormFieldsList(TEditorWindowType_Input, uint16_t(input), drawOrder, 1).size() == 1,
            "new input list does not open with one input");
        bool tooLong = false;
        try { editor.GetFormFieldsList(TEditorWindowType_Input, uint16_t(input), drawOrder, 2); } catch (const std::out_of_range&) { tooLong = true; }
        Check(tooLong, "input list form read past its table");
        // The toolbar runs Create new in one batch: a failure after the creation (here, opening the form) undoes it.
        Rejected(editor, [&] {
            EditorLogic::Mutation batch(editor, true);
            const auto id = editor.CreateNew(TEditorWindowType_Input);
            editor.GetFormFieldsList(TEditorWindowType_Input, uint16_t(id), drawOrder, 2);
            batch.Commit();
        });
    }
    {
        Editor editor(process, game, &info); Load(editor, true);
        Check(editor.CreateNew(TEditorWindowType_MovelistPlayable) == 2, "movelist playable not created");
        Check(editor.CreateNew(TEditorWindowType_MovelistInput) == 3, "movelist inputs not created");
    }
    {
        Editor editor(process, game, &info); Load(editor);
        editor.ModifyListSize(TEditorWindowType_Requirement, 1, {3, -1}, {1, 2});
        Verify(editor, {100, 103, 101}, 1);
    }
    {
        Editor editor(process, game, &info); Load(editor);
        editor.ModifyListSize(TEditorWindowType_Requirement, 0, {3, 1, 2, 0}, {});
        Verify(editor, {103, 101, 102, 100}, 0);
    }
    {
        Editor editor(process, game, &info); Load(editor);
        editor.DeleteStructures(TEditorWindowType_Requirement, 3, 1);
        Verify(editor, {100, 101, 102}, 2);
        editor.DeleteStructures(TEditorWindowType_Requirement, 0, 3);
        Verify(editor, {}, UINT64_MAX);
        editor.ModifyListSize(TEditorWindowType_Requirement, 0, {-1}, {});
        Verify(editor, {0}, UINT64_MAX);
    }
    {
        Editor editor(process, game, &info); Load(editor);
        Rejected(editor, [&] { editor.ModifyListSize(TEditorWindowType_Requirement, 1, {1, 1}, {}); });
        Rejected(editor, [&] { editor.ModifyListSize(TEditorWindowType_Requirement, 3, {3, 4}, {}); });
        Rejected(editor, [&] { editor.DeleteStructures(TEditorWindowType_Requirement, 1, SIZE_MAX); });
        editor.failAllocation = true;
        Rejected(editor, [&] { editor.DeleteStructures(TEditorWindowType_Requirement, 1, 1); });
        editor.failAllocation = false;
        Rejected(editor, [&] {
            editor.Mutate([&] {
                editor.DeleteStructures(TEditorWindowType_Requirement, 1, 1);
                throw std::bad_alloc();
            });
        });
    }
    {
        Editor editor(process, game, &info); Load(editor, true);
        editor.DeleteStructures(TEditorWindowType_MovelistInput, 1, 1);
        auto bytes = Snapshot(editor);
        auto header = reinterpret_cast<const TKMovesetHeader*>(bytes.data());
        auto blocks = reinterpret_cast<const StructsT7::TKMovesetHeaderBlocks*>(bytes.data() + header->block_list);
        auto payload = bytes.data() + header->moveset_data_start;
        Check(T7ImportUtils::ValidateT7Layout(*blocks, payload, bytes.size() - header->moveset_data_start), "movelist input deletion invalidated layout");
        auto mvl = reinterpret_cast<const StructsT7::MvlHead*>(payload + blocks->movelistBlock);
        auto sequences = reinterpret_cast<const StructsT7::MvlPlayable*>(reinterpret_cast<const Byte*>(mvl) + mvl->playables_offset);
        Check(sequences[0].input_count == 1 && sequences[1].input_sequence_offset == 1, "movelist sequence ranges did not follow deletion");
        editor.DeleteStructures(TEditorWindowType_MovelistPlayable, 0, 1);
        editor.ModifyListSize(TEditorWindowType_MovelistDisplayable, 0, {1}, {0});
        bytes = Snapshot(editor);
        header = reinterpret_cast<const TKMovesetHeader*>(bytes.data());
        blocks = reinterpret_cast<const StructsT7::TKMovesetHeaderBlocks*>(bytes.data() + header->block_list);
        payload = bytes.data() + header->moveset_data_start;
        mvl = reinterpret_cast<const StructsT7::MvlHead*>(payload + blocks->movelistBlock);
        Check(mvl->displayables_count == 1 && mvl->playables_count == 1, "movelist counts were adjusted twice");
        auto rows = reinterpret_cast<const StructsT7::MvlDisplayable*>(reinterpret_cast<const Byte*>(mvl) + mvl->displayables_offset);
        Check(rows[0].playable_id == 0 && strcmp(reinterpret_cast<const char*>(mvl) + rows[0].all_translation_offsets[0], "shared") == 0,
            "shared translation or playable reference was lost");
        Check(T7ImportUtils::ValidateT7Layout(*blocks, payload, bytes.size() - header->moveset_data_start), "displayable deletion invalidated layout");
    }
    {
        Editor editor(process, game, &info); Load(editor);
        auto copy = editor.DuplicateStructure(TEditorWindowType_Requirement, 1, 2);
        Check(copy == 4, "duplicate returned wrong ID");
        Verify(editor, {100, 101, 102, 103, 101, 102}, 3);
        Check(editor.CreateNew(TEditorWindowType_CancelExtradata) == 0, "empty-table insertion failed");
        Rejected(editor, [&] { editor.DuplicateStructure(TEditorWindowType_Requirement, 5, 3); });
        Rejected(editor, [&] { editor.DuplicateStructure(TEditorWindowType_Requirement, 0, 0); });
    }
    {
        Editor editor(process, game, &info); Load(editor);
        editor.DeleteStructures(TEditorWindowType_Input, 0, 3);
        auto bytes = Snapshot(editor);
        auto header = reinterpret_cast<const TKMovesetHeader*>(bytes.data());
        auto blocks = reinterpret_cast<const StructsT7::TKMovesetHeaderBlocks*>(bytes.data() + header->block_list);
        auto payload = bytes.data() + header->moveset_data_start;
        auto& info = const_cast<StructsT7_gameAddr::MovesetInfo&>(Info(bytes));
        auto sequence = reinterpret_cast<const StructsT7_gameAddr::InputSequence*>(payload + blocks->movesetBlock + info.table.inputSequence);
        Check(!info.table.inputCount && sequence->input_amount == 0 && sequence->input_addr == UINT64_MAX,
            "empty input sequence retained an invalid pointer");
        Check(T7ImportUtils::ConvertMovesetIndexes(payload, reinterpret_cast<gameAddr>(payload), &info.table, blocks),
            "edited references cannot be relocated");
    }
    {
        Editor editor(process, game, &info); Load(editor);
        editor.constants[EditorConstants_InputSequenceCommandStart] = 0x800d;
        editor.Mutate([&] {
            uint64_t size;
            auto bytes = const_cast<Byte*>(editor.GetMoveset(size));
            auto header = reinterpret_cast<TKMovesetHeader*>(bytes);
            auto blocks = reinterpret_cast<StructsT7::TKMovesetHeaderBlocks*>(bytes + header->block_list);
            auto info = reinterpret_cast<StructsT7_gameAddr::MovesetInfo*>(bytes + header->moveset_data_start);
            auto cancel = reinterpret_cast<StructsT7_gameAddr::Cancel*>(bytes + header->moveset_data_start + blocks->movesetBlock + info->table.cancel);
            cancel[0].command = 0xabcd0000800dULL;
        });
        Rejected(editor, [&] { editor.DeleteStructures(TEditorWindowType_InputSequence, 0, 1); });
    }
    {
        RecordEdit identity(0, {2, 0, 1}, {}, 3);
        Check(identity.RemapRange(0, 2) == std::pair<uint64_t,uint64_t>{1, 2}, "contiguous reordered sequence lost identity");
        RecordEdit edit(1, {2, 1}, {}, 4);
        Check(edit.RemapRange(0, 4) == std::pair<uint64_t,uint64_t>{0, 4}, "whole-sequence remap failed");
        bool rejected = false;
        try { RecordEdit split(0, {0, 2, 1}, {}, 3); split.RemapRange(0, 2); }
        catch (const std::invalid_argument&) { rejected = true; }
        Check(rejected, "split input sequence was silently accepted");
    }
}
