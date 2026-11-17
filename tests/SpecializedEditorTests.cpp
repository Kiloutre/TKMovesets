#include "Editor_t7/Editor_t7.hpp"
#include "Editor_t8/Editor_t8cnt.hpp"
#include "Importer_t7/Importer_t7_Utils.hpp"
#include "Animations.hpp"
#include "T7AnimToRetailT8.hpp"
#include "RetailT8AnimToT7.hpp"
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <set>
#include <stdexcept>
#include <vector>

namespace {
using namespace StructsT7;
void Check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
class Editor : public EditorT7 {
public:
    using EditorT7::EditorT7;
    int failAfter = -1;
    const auto& Names() const { return m_animNameToOffsetMap; }
    const auto& Offsets() const { return m_animOffsetToNameOffset; }
    auto Blocks() const { return reinterpret_cast<const TKMovesetHeaderBlocks*>(m_moveset + m_header->block_list); }
    auto Info() const { return reinterpret_cast<StructsT7_gameAddr::MovesetInfo*>(m_movesetData); }
    auto Moves() const { return reinterpret_cast<StructsT7_gameAddr::Move*>(m_movesetData + Blocks()->movesetBlock + Info()->table.move); }
    auto Movelist() const { return reinterpret_cast<MvlHead*>(m_movesetData + Blocks()->movelistBlock); }
    auto Rows() const { return reinterpret_cast<MvlDisplayable*>(reinterpret_cast<Byte*>(Movelist()) + Movelist()->displayables_offset); }
    std::string Name(uint64_t offset) const { return std::string(BlockString(TKMovesetHeaderBlocks_Name, offset)); }
    std::string Text(int row, int field) const { return std::string(BlockString(TKMovesetHeaderBlocks_Movelist, Rows()[row].all_translation_offsets[field])); }
    std::vector<Byte> Payload(int id) const {
        const auto offset = Moves()[id].anim_addr;
        const auto data = m_movesetData + Blocks()->animationBlock + offset;
        const auto size = TAnimUtils::FromMemory::Safe::GetAnimSize(data, Blocks()->motaBlock - Blocks()->animationBlock - offset);
        Check(size != 0, "edited animation payload is invalid");
        return {data, data + size};
    }
    void SeedMaps() {
        m_animNameToOffsetMap.clear(); m_animOffsetToNameOffset.clear();
        for (size_t i = 0; i < Info()->table.moveCount; ++i) {
            m_animNameToOffsetMap[Name(Moves()[i].anim_name_addr)] = Moves()[i].anim_addr;
            m_animOffsetToNameOffset[Moves()[i].anim_addr] = Moves()[i].anim_name_addr;
        }
    }
    void Replace(size_t block, size_t offset, size_t removed, std::span<const Byte> inserted) {
        Mutate([&] { ReplaceBlockRange(block, offset, removed, inserted); });
    }
protected:
    Byte* AllocateMutationBuffer(size_t size) override {
        if (failAfter == 0) { failAfter = -1; return nullptr; }
        if (failAfter > 0) --failAfter;
        return EditorT7::AllocateMutationBuffer(size);
    }
};
std::vector<Byte> Snapshot(Editor& editor) {
    uint64_t size; const auto bytes = editor.GetMoveset(size); return {bytes, bytes + size};
}
std::vector<Byte> Animation(Byte marker) {
    std::vector<Byte> bytes(36, 0);
    const uint16_t type = 0xc8, bones = 1;
    const uint32_t frames = 2;
    memcpy(bytes.data(), &type, 2); memcpy(bytes.data() + 2, &bones, 2);
    memcpy(bytes.data() + 4, &frames, 4); bytes[12] = marker;
    return bytes;
}
void Load(Editor& editor, bool emptyNames = false) {
    TKMovesetHeader header;
    header.header_size = header.block_list = sizeof(header);
    header.block_list_size = 8;
    header.moveset_data_start = sizeof(header) + sizeof(TKMovesetHeaderBlocks);
    TKMovesetHeaderBlocks blocks{};
    blocks.tableBlock = offsetof(MovesetInfo, table);
    blocks.motalistsBlock = offsetof(MovesetInfo, motas);
    blocks.nameBlock = sizeof(MovesetInfo);
    std::vector<Byte> names;
    std::vector<size_t> nameOffsets;
    for (const char* value : {"unused", "idle", "second", "last", "hero", "move_one", "move_two"}) {
        nameOffsets.push_back(names.size());
        names.insert(names.end(), value, value + strlen(value) + 1);
    }
    names.resize((names.size() + 7) & ~size_t(7), 0);
    blocks.movesetBlock = blocks.nameBlock + (emptyNames ? 0 : names.size());
    const size_t structureSize = emptyNames ? 0 : 3 * sizeof(StructsT7_gameAddr::Move);
    blocks.animationBlock = blocks.movesetBlock + structureSize;
    blocks.motaBlock = blocks.animationBlock + (emptyNames ? 0 : 120);
    blocks.movelistBlock = blocks.motaBlock + 8;
    const size_t displayables = 40, playables = displayables + 2 * sizeof(MvlDisplayable);
    const size_t inputs = playables + sizeof(MvlPlayable);
    const size_t size = header.moveset_data_start + blocks.movelistBlock + inputs + sizeof(MvlInput);
    auto bytes = static_cast<Byte*>(calloc(1, size)); if (!bytes) throw std::bad_alloc();
    memcpy(bytes, &header, sizeof(header)); memcpy(bytes + header.block_list, &blocks, sizeof(blocks));
    auto payload = bytes + header.moveset_data_start;
    auto info = reinterpret_cast<StructsT7_gameAddr::MovesetInfo*>(payload);
    memset(&info->motas, 0xff, sizeof(info->motas));
    memset(info->orig_aliases, 0xff, sizeof(info->orig_aliases));
    memset(info->current_aliases, 0xff, sizeof(info->current_aliases));
    for (auto& entry : info->table.entries) entry.listAddr = structureSize;
    info->table.move = 0; info->table.moveCount = emptyNames ? 0 : 3;
    if (!emptyNames) {
        memcpy(payload + blocks.nameBlock, names.data(), names.size());
        info->character_name_addr = reinterpret_cast<char*>(blocks.nameBlock + nameOffsets[4]);
        auto moves = reinterpret_cast<StructsT7_gameAddr::Move*>(payload + blocks.movesetBlock);
        for (int i = 0; i < 3; ++i) {
            moves[i].name_addr = nameOffsets[i == 0 ? 1 : i + 4];
            moves[i].anim_name_addr = nameOffsets[i + 1];
            moves[i].anim_addr = i * 40;
            moves[i].cancel_addr = moves[i]._0x28_cancel_addr = moves[i]._0x38_cancel_addr = moves[i]._0x48_cancel_addr = UINT64_MAX;
            moves[i].hit_condition_addr = moves[i].extra_move_property_addr = moves[i].move_start_extraprop_addr =
                moves[i].move_end_extraprop_addr = moves[i].voicelip_addr = UINT64_MAX;
            auto animation = Animation(static_cast<Byte>(i + 1));
            memcpy(payload + blocks.animationBlock + i * 40, animation.data(), animation.size());
        }
    }
    memcpy(payload + blocks.motaBlock, "MOTAdata", 8);
    auto mvl = reinterpret_cast<MvlHead*>(payload + blocks.movelistBlock);
    memcpy(mvl->mvlString, "MVLT", 4);
    mvl->displayables_count = 2; mvl->playables_count = 1;
    mvl->displayables_offset = displayables; mvl->playables_offset = playables; mvl->inputs_offset = inputs;
    memcpy(reinterpret_cast<Byte*>(mvl) + 32, "shared", 7);
    auto rows = reinterpret_cast<MvlDisplayable*>(reinterpret_cast<Byte*>(mvl) + displayables);
    for (int i = 0; i < 2; ++i) for (auto& offset : rows[i].all_translation_offsets) offset = 32;
    editor.hasDisplayableMovelist = true;
    editor.LoadMovesetPtr(bytes, size);
    editor.SeedMaps();
    editor.ReloadDisplayableMoveList();
    editor.mustReloadMovelist = false;
}
void Verify(Editor& editor) {
    auto bytes = Snapshot(editor);
    auto header = reinterpret_cast<const TKMovesetHeader*>(bytes.data());
    auto blocks = reinterpret_cast<const TKMovesetHeaderBlocks*>(bytes.data() + header->block_list);
    auto payload = bytes.data() + header->moveset_data_start;
    Check(T7ImportUtils::ValidateT7Layout(*blocks, payload, bytes.size() - header->moveset_data_start), "specialized edit produced invalid layout");
    Check(memcmp(payload + blocks->motaBlock, "MOTAdata", 8) == 0, "specialized edit damaged MOTA");
    for (size_t i = 0; i < editor.Info()->table.moveCount; ++i) {
        const auto& move = editor.Moves()[i];
        const auto name = editor.Name(move.anim_name_addr);
        Check(editor.Names().at(name) == move.anim_addr && editor.Offsets().contains(move.anim_addr), "animation lookup maps disagree with moves");
    }
}
struct Form {
    InputMap inputs;
    Form(TEditor& editor, EditorWindowType type, int id = 0) {
        VectorSet<std::string> order;
        inputs = type == TEditorWindowType_MovelistDisplayable ? editor.GetListSingleForm(type, id, order) : editor.GetFormFields(type, id, order);
        Check(!inputs.empty(), "test requested an unsupported form");
    }
    ~Form() = default;
    void Set(const std::string& key, const char* value) {
        auto field = inputs.at(key); strcpy_s(field->buffer, field->bufsize, value);
    }
};
template<class Operation> void Unchanged(Editor& editor, Operation operation) {
    const auto before = Snapshot(editor);
    uint64_t size; const auto original = editor.GetMoveset(size);
    const auto names = editor.Names(); const auto offsets = editor.Offsets();
    const auto rows = editor.displayableMovelist; const auto aliases = editor.aliases;
    const auto reload = editor.mustReloadMovelist, hasMovelist = editor.hasDisplayableMovelist;
    editor.live_loadedMoveset = 0x1234;
    operation();
    Check(Snapshot(editor) == before && editor.GetMoveset(size) == original, "failed/no-op edit changed original bytes or allocation");
    Check(editor.Names() == names && editor.Offsets() == offsets, "rollback lost animation maps");
    Check(editor.displayableMovelist == rows && editor.aliases == aliases &&
        editor.mustReloadMovelist == reload && editor.hasDisplayableMovelist == hasMovelist, "rollback lost cache identity or flags");
    Check(editor.live_loadedMoveset == 0x1234, "rollback lost live binding");
}
template<class Operation> void Rejected(Editor& editor, Operation operation) {
    Unchanged(editor, [&] {
        bool rejected = false;
        try { operation(); } catch (const std::exception&) { rejected = true; }
        Check(rejected, "invalid specialized edit was accepted");
    });
}

class T8Editor : public EditorT8CNT {
public:
    using EditorT8CNT::EditorT8CNT;
    int failAfter = -1;
    std::vector<Byte> Bytes() { uint64_t size; const auto p = GetMoveset(size); return {p, p + size}; }
    auto Movelist() {
        auto blocks = reinterpret_cast<const StructsT8CNT::TKMovesetHeaderBlocks*>(m_moveset + m_header->block_list);
        return reinterpret_cast<StructsT8CNT::MvlHead*>(m_movesetData + blocks->movelistBlock);
    }
    auto Rows() { return reinterpret_cast<StructsT8CNT::MvlDisplayable*>(reinterpret_cast<Byte*>(Movelist()) + Movelist()->displayables_offset); }
    std::string Text(int row, int field) { return std::string(BlockString(StructsT8CNT::TKMovesetHeaderBlocks_Movelist, Rows()[row].all_translation_offsets[field])); }
    void LoadTraining(Editor& source) {
        static_assert(sizeof(StructsT8CNT::MvlDisplayable) == sizeof(MvlDisplayable));
        static_assert(sizeof(StructsT8CNT::MvlPlayable) == sizeof(MvlPlayable));
        auto original = Snapshot(source);
        auto originalHeader = reinterpret_cast<const TKMovesetHeader*>(original.data());
        auto originalBlocks = reinterpret_cast<const TKMovesetHeaderBlocks*>(original.data() + originalHeader->block_list);
        const auto trainingSize = original.size() - originalHeader->moveset_data_start - originalBlocks->movelistBlock;
        auto header = *originalHeader;
        StructsT8CNT::TKMovesetHeaderBlocks blocks{};
        blocks.tableBlock = offsetof(StructsT8CNT::MovesetInfo, table);
        blocks.motalistsBlock = offsetof(StructsT8CNT::MovesetInfo, motas);
        blocks.nameBlock = blocks.movesetBlock = blocks.animationBlock = blocks.motaBlock = sizeof(StructsT8CNT::MovesetInfo);
        blocks.movelistBlock = blocks.motaBlock + 8;
        const auto size = header.moveset_data_start + blocks.movelistBlock + trainingSize;
        auto bytes = static_cast<Byte*>(calloc(1, size)); if (!bytes) throw std::bad_alloc();
        memcpy(bytes, &header, sizeof(header)); memcpy(bytes + header.block_list, &blocks, sizeof(blocks));
        auto payload = bytes + header.moveset_data_start;
        memcpy(payload + blocks.motaBlock, "MOTAdata", 8);
        memcpy(payload + blocks.movelistBlock, source.Movelist(), trainingSize);
        hasDisplayableMovelist = true;
        LoadMovesetPtr(bytes, size);
    }
protected:
    Byte* AllocateMutationBuffer(size_t size) override {
        if (failAfter == 0) { failAfter = -1; return nullptr; }
        if (failAfter > 0) --failAfter;
        return EditorT8CNT::AllocateMutationBuffer(size);
    }
};
class AnimationFile {
public:
    std::filesystem::path path;
    AnimationFile(std::string name, const std::vector<Byte>& bytes) {
        const auto directory = std::filesystem::temp_directory_path() / ("tkmovesets-edit-tests-" + std::to_string(GetCurrentProcessId()));
        std::filesystem::create_directories(directory);
        path = directory / (name + ".tkanim");
        std::ofstream file(path, std::ios::binary);
        file.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        Check(bool(file), "cannot write animation fixture");
    }
    ~AnimationFile() { std::error_code error; std::filesystem::remove(path, error); std::filesystem::remove(path.parent_path(), error); }
};
}

// Cancel graph invariants: one node per move, valid edges, the depth limit and the stops (generic moves are not
// expanded), incoming moves only lead to the root.
void CheckCancelGraph(Editor& editor, uint16_t root, int depth, bool incoming) {
    // LoadMovesetPtr leaves the constants to LoadMoveset: the T7 values (881 ends a requirement list)
    if (editor.constants.empty()) editor.constants = {
        {EditorConstants_RequirementEnd, 881}, {EditorConstants_CancelCommandEnd, 0x8000},
        {EditorConstants_GroupedCancelCommand, 0x800B}, {EditorConstants_GroupedCancelCommandEnd, 0x800C},
        {EditorConstants_InputSequenceCommandStart, 0x800D}, {EditorConstants_ProjectileProperty, 0x820B},
        {EditorConstants_ExtraProperty_Instant, 32769}, {EditorConstants_ExtraPropertyEnd, 0}};
    const auto graph = editor.BuildCancelGraph(root, depth, incoming);
    Check(!graph.nodes.empty() && graph.nodes[0].moveId == root && graph.nodes[0].depth == 0, "cancel graph root is wrong");
    Check(graph.nodes.size() <= 2 * CancelGraph::MaxNodes, "cancel graph exceeds its node limit");
    std::set<uint16_t> ids;
    for (const auto& node : graph.nodes) {
        Check(ids.insert(node.moveId).second, "cancel graph shows a move twice");
        Check(node.depth >= -1 && node.depth <= depth, "cancel graph node outside the depth limit");
        Check(node.depth >= 0 || incoming, "cancel graph has incoming moves it was not asked for");
    }
    std::set<std::pair<int, int>> pairs;
    for (const auto& edge : graph.edges) {
        Check(edge.from >= 0 && edge.to >= 0 && edge.from < int(graph.nodes.size()) && edge.to < int(graph.nodes.size()), "cancel graph edge out of range");
        Check(!edge.cancels.empty() && pairs.insert({edge.from, edge.to}).second, "cancel graph edge is empty or duplicated");
        const auto& from = graph.nodes[edge.from];
        if (from.depth < 0) Check(edge.to == 0, "incoming move does not lead to the root");
        // A generic move's own edge into the root comes from the incoming search, not from expanding it
        else Check(edge.from == 0 || !from.generic || (incoming && edge.to == 0), "cancel graph expanded a generic move");
        Check(from.depth < 0 || graph.nodes[edge.to].depth <= from.depth + 1, "cancel graph edge skips a level");
    }
}

void RunSpecializedEditorTests() {
    GameProcess process; GameData game; GameInfo info{}; info.dataString = info.minorDataString = "t7";
    {
        // Moves without a cancel list: the graph is the move alone
        Editor editor(process, game, &info); Load(editor);
        CheckCancelGraph(editor, 0, 5, true);
        Check(editor.BuildCancelGraph(0, 5, true).nodes.size() == 1 && editor.BuildCancelGraph(0, 5, true).edges.empty(), "move without cancels has a graph");
        Check(editor.BuildCancelGraph(999, 5, false).nodes.empty(), "cancel graph of a missing move");
    }
    {
        Editor editor(process, game, &info); Load(editor);
        const auto shared = editor.Moves()[0].name_addr;
        const auto metadata = reinterpret_cast<uint64_t>(editor.Info()->character_name_addr);
        Form form(editor, TEditorWindowType_Move);
        form.Set("move_name", "a longer renamed move");
        editor.SaveItem(TEditorWindowType_Move, 0, form.inputs);
        Check(editor.Name(editor.Moves()[0].name_addr) == "a longer renamed move", "growing move rename failed");
        Check(editor.Name(shared) == "idle" && editor.Moves()[0].anim_name_addr == shared, "move rename changed a shared animation name");
        Check(reinterpret_cast<uint64_t>(editor.Info()->character_name_addr) == metadata, "append relocated character metadata incorrectly");
        form.Set("move_name", "x");
        editor.SaveItem(TEditorWindowType_Move, 0, form.inputs);
        Check(editor.Name(editor.Moves()[0].name_addr) == "x", "shorter move rename failed");
        form.Set("move_name", "y");
        editor.SaveItem(TEditorWindowType_Move, 0, form.inputs);
        Check(editor.displayableMovelist[0]->name == "y", "same-length rename left stale cache");
        Unchanged(editor, [&] { editor.SaveItem(TEditorWindowType_Move, 0, form.inputs); });
        Verify(editor);
    }
    {
        Editor editor(process, game, &info); Load(editor);
        for (int allocation = 0; allocation < 2; ++allocation) {
            Form form(editor, TEditorWindowType_Move); form.Set("move_name", "renamed"); form.Set("vulnerability", "47");
            editor.failAfter = allocation;
            Rejected(editor, [&] { editor.SaveItem(TEditorWindowType_Move, 0, form.inputs); });
        }
        Form form(editor, TEditorWindowType_Move); form.Set("anim_name", "does not exist");
        Rejected(editor, [&] { editor.SaveItem(TEditorWindowType_Move, 0, form.inputs); });
        form.Set("anim_name", "idle");
        auto field = form.inputs.at("move_name");
        memset(field->buffer, 'x', field->bufsize);
        Rejected(editor, [&] { editor.SaveItem(TEditorWindowType_Move, 0, form.inputs); });
        InputMap missing;
        Rejected(editor, [&] { editor.SaveItem(TEditorWindowType_Move, 0, missing); });
        Rejected(editor, [&] { editor.SaveItem(TEditorWindowType_Move, 99, missing); });
    }
    {
        Editor editor(process, game, &info); Load(editor);
        Form form(editor, TEditorWindowType_Move); form.Set("move_name", "pending");
        bool completed = false;
        Rejected(editor, [&] {
            EditorLogic::Mutation outer(editor);
            editor.SaveItem(TEditorWindowType_Move, 0, form.inputs);
            editor.aliases.push_back(99);
            editor.hasDisplayableMovelist = false;
            completed = true;
            throw std::bad_alloc();
        });
        Check(completed, "inner move edit never completed");
        Check(editor.displayableMovelist[0]->name == "idle", "rollback restored mutated row content");
    }
    {
        Editor editor(process, game, &info); Load(editor);
        const auto metadata = reinterpret_cast<uint64_t>(editor.Info()->character_name_addr);
        for (int allocation = 0; allocation < 2; ++allocation) {
            editor.failAfter = allocation;
            Rejected(editor, [&] { editor.DeleteNameBlockString(0); });
        }
        editor.DeleteNameBlockString(0);
        Check(editor.Name(metadata - editor.Blocks()->nameBlock - 7) == "hero" &&
            reinterpret_cast<uint64_t>(editor.Info()->character_name_addr) == metadata - 7, "name deletion did not relocate metadata");
        Rejected(editor, [&] { editor.DeleteNameBlockString(editor.Moves()[0].name_addr); });
        Rejected(editor, [&] { editor.DeleteNameBlockString(editor.Moves()[0].name_addr + 1); });
        Rejected(editor, [&] { editor.DeleteNameBlockString(metadata - editor.Blocks()->nameBlock - 7); });
        Rejected(editor, [&] { editor.DeleteNameBlockString(UINT64_MAX); });
        Rejected(editor, [&] { editor.DeleteAnimation(0); });
        Rejected(editor, [&] { editor.DeleteAnimation(1); });
        Rejected(editor, [&] { editor.DeleteAnimation(UINT64_MAX); });
        Verify(editor);
    }
    {
        Editor editor(process, game, &info); Load(editor);
        Form form(editor, TEditorWindowType_MovelistDisplayable);
        form.Set("title_translation_0", "different title {1}");
        editor.SaveItem(TEditorWindowType_MovelistDisplayable, 0, form.inputs);
        Check(editor.Text(0, 0) != "shared", "movelist text was overwritten by an unchanged shared field");
        for (int i = 1; i < 16; ++i) Check(editor.Text(0, i) == "shared", "text edit changed another language");
        for (int i = 0; i < 16; ++i) Check(editor.Text(1, i) == "shared", "text edit changed another row");
        Form again(editor, TEditorWindowType_MovelistDisplayable);
        Unchanged(editor, [&] { editor.SaveItem(TEditorWindowType_MovelistDisplayable, 0, again.inputs); });
        editor.Rows()[0].all_translation_offsets[0] = 0;
        Form blank(editor, TEditorWindowType_MovelistDisplayable); blank.Set("title_translation_0", "");
        editor.SaveItem(TEditorWindowType_MovelistDisplayable, 0, blank.inputs);
        Check(editor.Rows()[0].all_translation_offsets[0] != 0 && editor.Text(0, 0).empty(), "new empty translation corrupted header");
        Verify(editor);
    }
    {
        Editor editor(process, game, &info); Load(editor);
        for (int allocation = 0; allocation < 3; ++allocation) {
            Form form(editor, TEditorWindowType_MovelistDisplayable);
            form.Set("icons", "0x123"); form.Set("title_translation_0", "first change"); form.Set("translation_0", "second change");
            editor.failAfter = allocation;
            Rejected(editor, [&] { editor.SaveItem(TEditorWindowType_MovelistDisplayable, 0, form.inputs); });
        }
        Form form(editor, TEditorWindowType_MovelistDisplayable);
        editor.Rows()[0].all_translation_offsets[0] = INT32_MAX;
        Rejected(editor, [&] { editor.SaveItem(TEditorWindowType_MovelistDisplayable, 0, form.inputs); });
    }
    {
        Editor editor(process, game, &info); Load(editor, true);
        const std::vector<Byte> name{'f','i','r','s','t',0};
        editor.Replace(TKMovesetHeaderBlocks_Name, 0, 0, name);
        Check(editor.Name(0) == "first", "empty name section append failed at offset zero");
        Verify(editor);
        Rejected(editor, [&] { editor.Replace(TKMovesetHeaderBlocks_Name, SIZE_MAX, 1, {}); });
        Rejected(editor, [&] { editor.Replace(TKMovesetHeaderBlocks_Name, 0, SIZE_MAX, {}); });
        std::vector<Byte> oversized(256ULL << 20);
        Rejected(editor, [&] { editor.Replace(TKMovesetHeaderBlocks_Name, 0, 0, oversized); });
    }

    {
        // This tests existing CNT record plumbing; it makes no claim about game support.
        Editor source(process, game, &info); Load(source, true);
        T8Editor editor(process, game, &info); editor.LoadTraining(source);
        Form form(editor, TEditorWindowType_MovelistDisplayable);
        form.Set("title_translation_0", "CNT title"); form.Set("translation_0", "CNT translation");
        const auto before = editor.Bytes();
        editor.failAfter = 2;
        editor.live_loadedMoveset = 0x1234;
        bool rejected = false;
        try { editor.SaveItem(TEditorWindowType_MovelistDisplayable, 0, form.inputs); }
        catch (const std::bad_alloc&) { rejected = true; }
        Check(rejected && editor.Bytes() == before && editor.live_loadedMoveset == 0x1234, "CNT string edit did not roll back");
        editor.SaveItem(TEditorWindowType_MovelistDisplayable, 0, form.inputs);
        Check(editor.Text(0, 0) == "CNT title" && editor.Text(0, 4) == "CNT translation" && editor.Text(1, 0) == "shared",
            "CNT text edit changed shared storage");
        const auto after = editor.Bytes();
        Check(editor.ImportAnimation(L"unused", 0).empty() && editor.Bytes() == after, "unsupported CNT animation import changed bytes");
        rejected = false;
        try { editor.DeleteAnimation(0); } catch (const std::logic_error&) { rejected = true; }
        Check(rejected && editor.Bytes() == after, "unsupported CNT animation deletion changed bytes");
    }
    AnimationFile last("different-filename", Animation(3));
    AnimationFile fresh("fresh", Animation(9));
    AnimationFile malformed("malformed", {0x64, 0, 1, 0, 0, 0, 0, 0});
    {
        Editor editor(process, game, &info); Load(editor);
        Check(editor.ImportAnimation(last.path.c_str(), 0) == "last", "final-animation duplicate did not return canonical name");
        Check(editor.Moves()[0].anim_addr == editor.Moves()[2].anim_addr, "dedup did not reuse final animation");
        Check(editor.Name(editor.Moves()[0].name_addr) == "idle", "animation cleanup deleted a move's shared name");
        Check(!editor.Names().contains("idle"), "animation cleanup kept a stale lookup");
        Check(editor.ImportAnimation(last.path.c_str(), 1) == "last", "second final-animation dedup failed");
        Check(editor.Offsets().size() == 1, "dedup left an unused animation");
        Unchanged(editor, [&] { Check(editor.ImportAnimation(last.path.c_str(), 0) == "last", "only-animation dedup/no-op failed"); });
        Verify(editor);
        Check(!editor.ImportAnimation(fresh.path.c_str(), 1).empty(), "new animation import failed");
        Check(editor.Payload(1) == Animation(9), "new import installed the wrong animation bytes");
        Verify(editor);
    }
    {
        Editor editor(process, game, &info); Load(editor);
        auto big = Animation(8); Check(TAnimUtils::FromMemory::ByteswapAnimation(big.data(), big.size()), "fixture endian conversion failed");
        big.insert(big.end(), 15, 0xaa);
        AnimationFile padded("idle", big);
        Check(editor.ImportAnimation(padded.path.c_str(), 0) == "idle (2)", "big-endian import or name collision handling failed");
        Check(editor.Payload(0) == Animation(8), "import failed to normalize/truncate animation bytes");
        Verify(editor);
        Unchanged(editor, [&] { Check(editor.ImportAnimation(malformed.path.c_str(), 0).empty(), "invalid file imported"); });
        Unchanged(editor, [&] { Check(editor.ImportAnimation(L"missing-animation-file", 0).empty(), "missing file imported"); });
        Unchanged(editor, [&] { Check(editor.ImportAnimation(fresh.path.c_str(), -1).empty(), "negative move ID imported"); });
        Unchanged(editor, [&] { Check(editor.ImportAnimation(fresh.path.c_str(), 99).empty(), "invalid move ID imported"); });
        // Retail T8 PANM (raw tracks): converted to a T7 0xC8 body animation on import, deduplicated like any other.
        RetailPanm::T7Animation t7{4, 23, std::vector<float>(4 * 23 * 3), false};
        for (size_t i = 0; i < t7.values.size(); ++i) t7.values[i] = static_cast<float>(std::sin(i * 0.1));
        const auto panm = T7AnimToRetailT8::Convert(t7);
        AnimationFile panmFile("t8-move", panm);
        Check(editor.ImportAnimation(panmFile.path.c_str(), 0) == "t8-move", "PANM animation import failed");
        Check(editor.Payload(0) == RetailT8AnimToT7::Convert(panm, RetailT8AnimToT7::Format::Frames), "PANM import installed the wrong 0xC8 bytes");
        Verify(editor);
        Check(editor.ImportAnimation(panmFile.path.c_str(), 1) == "t8-move" && editor.Moves()[0].anim_addr == editor.Moves()[1].anim_addr, "PANM import was not deduplicated");
        auto truncated = panm; truncated.resize(truncated.size() / 2);
        AnimationFile brokenPanm("broken-panm", truncated);
        Unchanged(editor, [&] { Check(editor.ImportAnimation(brokenPanm.path.c_str(), 0).empty(), "invalid PANM imported"); });
    }
    {
        Editor editor(process, game, &info); Load(editor);
        bool completed = false;
        Rejected(editor, [&] {
            EditorLogic::Mutation outer(editor);
            Check(!editor.ImportAnimation(fresh.path.c_str(), 0).empty(), "nested import failed");
            completed = true;
            throw std::bad_alloc();
        });
        Check(completed, "inner animation import never completed");
    }
    {
        Editor editor(process, game, &info); Load(editor);
        // Fail each allocation across snapshot, name append, animation append and nested cleanup.
        int rejected = 0;
        bool completed = false;
        for (int allocation = 0; allocation < 12; ++allocation) {
            editor.failAfter = allocation;
            bool failed = false;
            const auto before = Snapshot(editor);
            // Once the complete import fits the allocation schedule, stop this fixture.
            try {
                Unchanged(editor, [&] {
                    const auto result = editor.ImportAnimation(fresh.path.c_str(), 0);
                    failed = result.empty();
                    if (!failed) throw std::logic_error("import succeeded");
                });
            } catch (const std::logic_error&) {
                Check(!failed, "unexpected fault test failure");
                editor.failAfter = -1; Verify(editor); completed = true; break;
            }
            Check(failed && Snapshot(editor) == before, "failed import changed bytes");
            ++rejected;
        }
        Check(completed && rejected > 0, "import fault schedule did not reach success after testing every allocation");
    }
}

void RunHistoricalEditorTests(const std::vector<Byte>& file)
{
    GameProcess process; GameData game; GameInfo info{}; info.dataString = info.minorDataString = "t7";
    Editor editor(process, game, &info);
    auto bytes = static_cast<Byte*>(malloc(file.size())); if (!bytes) throw std::bad_alloc();
    memcpy(bytes, file.data(), file.size());
    auto header = reinterpret_cast<const TKMovesetHeader*>(bytes);
    auto blocks = reinterpret_cast<const TKMovesetHeaderBlocks*>(bytes + header->block_list);
    editor.hasDisplayableMovelist = file.size() - header->moveset_data_start - blocks->movelistBlock >= 4 &&
        memcmp(bytes + header->moveset_data_start + blocks->movelistBlock, "MVLT", 4) == 0;
    editor.LoadMovesetPtr(bytes, file.size()); editor.SeedMaps(); editor.ReloadDisplayableMoveList();
    if (!editor.Info()->table.moveCount) return;
    // Cancel graphs of the first moves and of every generic move (idle and the like fan out the most)
    try {
        for (uint16_t move = 0; move < (std::min)(uint32_t(8), editor.MoveCount()); ++move) CheckCancelGraph(editor, move, 5, true);
        for (auto alias : editor.aliases) if (alias < editor.MoveCount()) CheckCancelGraph(editor, alias, CancelGraph::MaxNodes, alias == editor.aliases[0]);
    } catch (const std::exception& e) { throw std::runtime_error(std::string("cancel graph: ") + e.what()); }
    Form form(editor, TEditorWindowType_Move);
    form.Set("move_name", "audit regression rename");
    bool completed = false;
    Rejected(editor, [&] {
        EditorLogic::Mutation outer(editor);
        editor.SaveItem(TEditorWindowType_Move, 0, form.inputs);
        auto changed = Snapshot(editor);
        auto changedHeader = reinterpret_cast<const TKMovesetHeader*>(changed.data());
        auto changedBlocks = reinterpret_cast<const TKMovesetHeaderBlocks*>(changed.data() + changedHeader->block_list);
        Check(T7ImportUtils::ValidateT7Layout(*changedBlocks, changed.data() + changedHeader->moveset_data_start,
            changed.size() - changedHeader->moveset_data_start), "historical move rename invalidated layout");
        Check(editor.Name(editor.Moves()[0].name_addr) == "audit regression rename", "historical rename did not apply");
        completed = true;
        throw std::bad_alloc();
    });
    Check(completed, "historical rename failed before the planned rollback");
}
