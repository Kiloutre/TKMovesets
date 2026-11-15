#include "Editor_t7/Editor_t7.hpp"
#include "MovesetFile.hpp"
#include "Compression.hpp"
#include "FileIO.hpp"
#include "FileJob.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <stdexcept>

std::vector<Byte> SemanticFixture();
namespace {
namespace fs = std::filesystem;
void Check(bool value, const char* error) { if (!value) throw std::runtime_error(error); }
class Editor : public EditorT7 {
public:
    using EditorT7::EditorT7;
    size_t allocations = 0, allocationBytes = 0;
    auto Info() const { return reinterpret_cast<const StructsT7_gameAddr::MovesetInfo*>(m_movesetData); }
    auto Blocks() const { return reinterpret_cast<const StructsT7::TKMovesetHeaderBlocks*>(m_moveset + m_header->block_list); }
    auto Moves() const { return reinterpret_cast<const StructsT7_gameAddr::Move*>(m_movesetData + Blocks()->movesetBlock + Info()->table.move); }
    const auto& Names() const { return m_animNameToOffsetMap; }
protected:
    Byte* AllocateMutationBuffer(size_t size) override {
        ++allocations; allocationBytes += size;
        return EditorT7::AllocateMutationBuffer(size);
    }
};
struct Form {
    InputMap fields;
    Form(Editor& editor, EditorWindowType type, uint16_t id) {
        VectorSet<std::string> order;
        fields = type == TEditorWindowType_Move ? editor.GetFormFields(type, id, order) : editor.GetListSingleForm(type, id, order);
    }
    ~Form() = default;
    void Set(const char* key, const std::string& text) { auto f = fields.at(key); strcpy_s(f->buffer, f->bufsize, text.c_str()); }
};
std::vector<Byte> Bytes(Editor& editor) { uint64_t size; auto data = editor.GetMoveset(size); return {data, data + size}; }
void Load(Editor& editor, const std::vector<Byte>& bytes) {
    auto copy = static_cast<Byte*>(malloc(bytes.size())); if (!copy) throw std::bad_alloc();
    memcpy(copy, bytes.data(), bytes.size());
    Check(editor.LoadMoveset(copy, bytes.size()), "workflow editor load failed");
    editor.ReloadDisplayableMoveList();
}
struct Directory {
    fs::path path = fs::temp_directory_path() / ("tkmovesets-workflow-" + std::to_string(GetCurrentProcessId()));
    Directory() { Check(fs::create_directory(path), "cannot create workflow directory"); }
    ~Directory() { std::error_code error; fs::remove_all(path, error); }
};
void Apply(Editor& editor, const fs::path& animation) {
    editor.ModifyListSize(TEditorWindowType_Requirement, 0, {-1, 0, 1}, {});
    Form requirement(editor, TEditorWindowType_Requirement, 0);
    requirement.Set("condition", "100"); requirement.Set("param_unsigned", "73");
    editor.SaveItem(TEditorWindowType_Requirement, 0, requirement.fields);
    Form move(editor, TEditorWindowType_Move, 0);
    move.Set("move_name", "workflow move"); move.Set("vulnerability", "47");
    editor.SaveItem(TEditorWindowType_Move, 0, move.fields);
    Form training(editor, TEditorWindowType_MovelistDisplayable, 0);
    training.Set("title_translation_0", "workflow title");
    editor.SaveItem(TEditorWindowType_MovelistDisplayable, 0, training.fields);
    Check(editor.ImportAnimation(animation.c_str(), 0) == "workflow-animation", "workflow animation import failed");
}
void Verify(Editor& editor) {
    Check(editor.Info()->table.requirementCount == 3, "workflow record insertion lost");
    Form requirement(editor, TEditorWindowType_Requirement, 0);
    Check(std::string(requirement.fields.at("param_unsigned")->buffer) == "73", "workflow numeric change lost");
    Form move(editor, TEditorWindowType_Move, 0);
    Check(std::string(move.fields.at("move_name")->buffer) == "workflow move", "workflow move name lost");
    Check(std::string(move.fields.at("anim_name")->buffer) == "workflow-animation", "workflow animation reference lost");
    Check(std::string(move.fields.at("vulnerability")->buffer) == "47", "workflow move scalar lost");
    Form training(editor, TEditorWindowType_MovelistDisplayable, 0);
    Check(std::string(training.fields.at("title_translation_0")->buffer) == "workflow title", "workflow translation lost");
    Check(std::string(training.fields.at("translation_0")->buffer) == "text", "workflow shared translation corrupted");
}
}
void BenchmarkEditor(const fs::path& path) {
    MovesetFile file(path);
    auto bytes = file.ConvertToT7();
    GameAddressesFile addresses; GameProcess process; GameData game; game.SetProcess(&process, &addresses);
    GameInfo info{}; info.dataString = info.minorDataString = "t7";
    Editor editor(process, game, &info); Load(editor, bytes);
    Form field(editor, TEditorWindowType_Requirement, 0);
    editor.allocations = editor.allocationBytes = 0;
    auto start = std::chrono::steady_clock::now();
    {
        EditorLogic::Mutation transaction(editor, true);
        for (int i = 0; i < 100; ++i) {
            field.Set("param_unsigned", std::to_string(i));
            editor.SaveItem(TEditorWindowType_Requirement, 0, field.fields);
        }
        transaction.Commit();
    }
    const auto elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    std::cout << nlohmann::json({{"file", path.string()}, {"edits", 100}, {"payload_bytes", bytes.size()},
        {"allocations", editor.allocations}, {"allocated_bytes", editor.allocationBytes}, {"milliseconds", elapsed}}).dump() << '\n';
}
void RunEditorWorkflowTests() {
    Directory directory;
    const auto original = SemanticFixture();
    const auto source = directory.path / "source.tkmvst";
    Check(FileIO::WriteAtomically(source, original), "workflow source write failed");
    std::vector<Byte> animation(36);
    const uint16_t type = 0xc8, bones = 1; const uint32_t frames = 2;
    memcpy(animation.data(), &type, 2); memcpy(animation.data() + 2, &bones, 2);
    memcpy(animation.data() + 4, &frames, 4); animation[12] = 9;
    const auto animationPath = directory.path / "workflow-animation.tkanim";
    Check(FileIO::WriteAtomically(animationPath, animation), "workflow animation write failed");
    GameAddressesFile addresses; GameProcess process; GameData game; game.SetProcess(&process, &addresses);
    GameInfo info{}; info.dataString = info.minorDataString = "t7";
    Editor editor(process, game, &info); Load(editor, original);
    const auto before = Bytes(editor);
    const auto names = editor.Names();
    const auto rows = editor.displayableMovelist;
    uint64_t size; const auto pointer = editor.GetMoveset(size);
    editor.live_loadedMoveset = 0x1234;
    bool reachedEnd = false;
    try {
        EditorLogic::Mutation transaction(editor, true);
        Apply(editor, animationPath);
        reachedEnd = true;
        throw std::bad_alloc();
    } catch (const std::bad_alloc&) {}
    Check(reachedEnd, "workflow failed before injected rollback");
    Check(Bytes(editor) == before && editor.GetMoveset(size) == pointer && editor.displayableMovelist == rows &&
        editor.Names() == names && editor.live_loadedMoveset == 0x1234, "compound workflow rollback lost state");
    {
        bool rejected = false;
        try {
            EditorLogic::Mutation batch(editor, true);
            Form field(editor, TEditorWindowType_Requirement, 0);
            field.Set("param_unsigned", "99");
            editor.SaveItem(TEditorWindowType_Requirement, 0, field.fields);
            try { editor.DeleteStructures(TEditorWindowType_Requirement, UINT32_MAX, 1); }
            catch (const std::exception&) {}
            batch.Commit();
        } catch (const std::exception&) { rejected = true; }
        Check(rejected && Bytes(editor) == before && editor.GetMoveset(size) == pointer, "caught child failure committed partial batch");
    }
    {
        EditorLogic::Mutation batch(editor, true);
        Form field(editor, TEditorWindowType_Requirement, 0);
        field.Set("param_unsigned", "12"); editor.SaveItem(TEditorWindowType_Requirement, 0, field.fields);
        const auto intermediate = Bytes(editor);
        try {
            EditorLogic::Mutation savepoint(editor);
            field.Set("param_unsigned", "98"); editor.SaveItem(TEditorWindowType_Requirement, 0, field.fields);
            throw std::bad_alloc();
        } catch (const std::bad_alloc&) {}
        Check(Bytes(editor) == intermediate, "independent nested savepoint did not restore parent work");
        // Leave the successful outer batch uncommitted to restore the original fixture.
    }
    Check(Bytes(editor) == before && editor.GetMoveset(size) == pointer, "outer batch rollback failed after a savepoint");
    {
        Form field(editor, TEditorWindowType_Requirement, 0);
        editor.allocations = editor.allocationBytes = 0;
        {
            EditorLogic::Mutation batch(editor, true);
            for (int i = 0; i < 100; ++i) {
                field.Set("param_unsigned", std::to_string(i));
                editor.SaveItem(TEditorWindowType_Requirement, 0, field.fields);
            }
            Check(editor.allocationBytes <= 2 * before.size(), "scalar batch copied the file for every row");
        }
        Check(Bytes(editor) == before, "performance batch changed data after rollback");
    }
    Apply(editor, animationPath); Verify(editor);
    const auto liveBytes = Bytes(editor);
    auto edited = editor.BuildSaveSnapshot();
    Check(Bytes(editor) == liveBytes, "saving changed live editor metadata");
    for (auto codec : {TKMovesetCompressionType_None, TKMovesetCompressionType_LZ4, TKMovesetCompressionType_LZMA}) {
        const auto target = directory.path / ("saved-" + std::to_string(codec) + ".tkmvst");
        FileJobRequest request; request.output = target; request.bytes = edited; request.codec = codec;
        Check(RunFileJob(std::move(request)).ok, "workflow save job failed");
        MovesetFile saved(target); saved.Validate();
        Check(saved.Bytes().size() == edited.size() && !memcmp(saved.Bytes().data(), edited.data(), edited.size()), "saved workflow bytes differ");
        Editor reloaded(process, game, &info); Load(reloaded, {saved.Bytes().begin(), saved.Bytes().end()}); Verify(reloaded);
        auto lock = CreateFileW(target.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
        Check(lock != INVALID_HANDLE_VALUE, "cannot lock workflow output");
        FileJobRequest failed; failed.output = target; failed.bytes = edited; failed.codec = codec;
        const auto result = RunFileJob(std::move(failed));
        Check(!result.ok && !result.error.empty(), "locked destination was replaced");
        CloseHandle(lock);
        MovesetFile preserved(target); preserved.Validate();
        Check(!memcmp(preserved.Bytes().data(), edited.data(), edited.size()), "failed save lost existing output");
    }
    MovesetFile preservedSource(source);
    Check(preservedSource.Bytes().size() == original.size() && !memcmp(preservedSource.Bytes().data(), original.data(), original.size()),
        "workflow changed source file");
}
