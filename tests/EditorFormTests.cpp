#include "GameSharedMem.hpp"
#include "EditorFormList.hpp"
#include "EditorVisuals_t7/Structures/TEditorStructures.hpp"
#include "EditorVisuals.hpp"
#include <sstream>
#include <iomanip>
#include "EditorClipboard.hpp"
#include "Editor_t7/Editor_t7.hpp"
#include <stdexcept>
#include <cstring>
#include <type_traits>

namespace {
void Check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
template<class F> void Reject(F&& action) {
    bool rejected = false;
    try { action(); } catch (const std::exception&) { rejected = true; }
    Check(rejected, "Invalid editor operation was accepted");
}
OwnedEditorInput Input(const char* text) {
    auto buffer = std::make_unique<char[]>(8);
    OwnedEditorInput field(new EditorInput{});
    field->bufsize = 8; field->buffer = buffer.release();
    strcpy_s(field->buffer, field->bufsize, text);
    field->visible = false;
    return field;
}
// A real editor subclass with an inert byte store: no process or live-edit API is called.
class FixtureEditor : public EditorT7 {
public:
    int validations = 0, saves = 0;
    bool writeSaves = false, failAllocation = false;
    Byte* AllocateMutationBuffer(size_t size) override {
        return failAllocation ? nullptr : EditorT7::AllocateMutationBuffer(size);
    }
    int failSaveAt = -1, failValidationAt = -1, recomputes = 0;
    bool failRecompute = false, poisonRecompute = false;
    void RecomputeDisplayableMoveFlags(uint16_t) override {
        ++recomputes;
        Check(live_loadedMoveset == 0, "Move callback retained live binding");
        if (!displayableMovelist.empty()) {
            displayableMovelist[0]->name = "callback cache";
            displayableMovelist[0]->flags ^= EditorMoveFlags_Attack;
        }
        if (failRecompute) throw std::runtime_error("move flag callback failed");
        if (poisonRecompute) {
            try { Mutate([&] { Touch(); throw std::runtime_error("caught child failure"); }); }
            catch (const std::exception&) {}
        }
    }
    void Touch() { ++m_moveset[0]; }
    auto ByteValue() const { return m_moveset[0]; }
    auto BytePointer() const { return m_moveset; }
    void ModifyListSize(EditorWindowType, unsigned int, const std::vector<int>&, const std::set<int>&) override {
        if (writeSaves) Touch();
    }
    bool failDelete = false, failDuplicate = false, noStructuralChange = false;
    uint32_t duplicateResult = 22;
    size_t duplicatedAmount = 0;
    uint32_t DuplicateStructure(EditorWindowType, uint32_t, size_t amount) override {
        duplicatedAmount = amount; if (!noStructuralChange) Touch();
        if (failDuplicate) throw std::runtime_error("duplication engine failure");
        return duplicateResult;
    }
    void DeleteStructures(EditorWindowType, uint32_t, size_t) override {
        if (!noStructuralChange) Touch(); if (failDelete) throw std::runtime_error("deletion engine failure");
    }
    bool failNewRow = false;
    enum Mode { Normal, Empty, MissingOrder, NullField, Unterminated, WrongCount, LateFactoryFailure, LateRowFailure } mode = Normal;
    InputMap GetFormFields(EditorWindowType, uint16_t, VectorSet<std::string>& order) override {
        InputMap fields;
        if (mode == Empty) return fields;
        fields.Own("b", Input("new"));
        order.push_back("b");
        if (mode == LateFactoryFailure) throw std::runtime_error("field factory failed after allocation");
        if (mode == MissingOrder) { order.push_back("absent"); return fields; }
        if (mode == NullField) { delete[] fields.at("b")->buffer; fields.at("b")->buffer = nullptr; }
        if (mode == Unterminated) memset(fields.at("b")->buffer, 'x', fields.at("b")->bufsize);
        return fields;
    }
    std::vector<InputMap> GetFormFieldsList(EditorWindowType type, uint16_t id, VectorSet<std::string>& order, int) override {
        std::vector<InputMap> rows;
        rows.push_back(GetFormFields(type, id, order));
        if (mode == LateRowFailure) throw std::runtime_error("row factory failed after allocation");
        if (mode != WrongCount) rows.push_back(GetFormFields(type, id, order));
        return rows;
    }
    std::vector<InputMap> GetFormFieldsList(EditorWindowType type, uint16_t id, VectorSet<std::string>& order) override {
        return GetFormFieldsList(type, id, order, 2);
    }
    std::vector<std::string> saved;
    FixtureEditor(GameProcess& process, GameData& game, const GameInfo* info) : EditorT7(process, game, info) { LoadMovesetPtr(static_cast<Byte*>(calloc(1, 1)), 1); }
    void LoadMovesetPtr(Byte* bytes, uint64_t size) override { m_moveset = bytes; m_movesetSize = size; }
    InputMap GetListSingleForm(EditorWindowType, uint16_t, VectorSet<std::string>& order) override {
        if (failNewRow) throw std::runtime_error("row initialization failed");
        order.push_back("b"); order.push_back("a");
        InputMap fields; fields.Own("b", Input("new")); fields.Own("a", Input("0")); return fields;
    }
    bool ValidateField(EditorWindowType, EditorInput* field) override {
        ++validations;
        if (validations == failValidationAt) { Touch(); throw std::runtime_error("late validation failure"); }
        return std::string(field->buffer) != "bad";
    }
    void SaveItem(EditorWindowType, uint16_t, InputMap& inputs) override {
        ++saves;
        if (writeSaves) Touch();
        if (saves == failSaveAt) throw std::runtime_error("late save failure");
        for (auto& [key, field] : inputs) saved.emplace_back(field->buffer);
    }
};
class Form : public EditorForm {
public:
    int updates = 0;
    Form(FixtureEditor& editor) {
        m_editor = &editor; m_windowTypeName = "test";
        m_fieldIdentifierMap.Own("b", Input("old")); m_fieldIdentifierMap.Own("a", Input("end"));
        for (auto& [key, field] : m_fieldIdentifierMap) m_fieldsCategoryMap[0].push_back(field);
    }
    using EditorForm::PasteText;
    using EditorForm::Apply;
    auto Field(const char* key) { return m_fieldIdentifierMap.at(key); }
    bool failObservation = false;
    std::string Signature() const {
        std::ostringstream out;
        out << structureId << popen << unsavedChanges << justAppliedChanges << moveFocusAway << m_requestedClosure << m_windowTitle;
        for (const auto& [key, field] : m_fieldIdentifierMap)
            out << field << static_cast<void*>(field->buffer) << field->buffer << field->flags << field->errored
                << field->displayName << bool(field->nextValue) << field->nextValue.value_or("");
        return out.str();
    }
    void RequestFieldUpdate(EditorWindowType, int, int, int) override {
        ++structureId; popen = false; unsavedChanges = true; justAppliedChanges = true;
        moveFocusAway = true; m_requestedClosure = true; m_windowTitle = "observer changed";
        Field("b")->QueueValue("queued"); strcpy_s(Field("a")->buffer, 8, "shifted");
        Field("a")->displayName = "observer label"; Field("a")->errored = true;
        if (failObservation) throw std::runtime_error("late observer failure");
    }
    void OnUpdate(int, EditorInput*) override { ++updates; }
};
class SingleForm : public Form {
public:
    using Form::Form;
    using EditorForm::TryApply;
    int applies = 0;
    enum Failure { None, FirstUpdate, LateUpdate, FinalApply, Reenter, UnknownApply, PoisonCommit, UpdateBytes } failure = None;
    std::vector<EditorForm*> observers;
    auto Error() const { return m_operationError; }
    std::unique_ptr<TransactionState> BeginEditTransaction() override {
        std::vector<EditorForm*> forms = observers; forms.push_back(this); forms.push_back(this);
        return EditorVisuals::BeginFormTransaction(forms);
    }
    std::string Signature() const {
        std::ostringstream out; out << Form::Signature() << setFocus;
        for (auto category : m_categories) out << int(category);
        for (const auto& [category, label] : m_categoryStringIdentifiers) out << category << label;
        for (const auto& [category, fields] : m_fieldsCategoryMap) {
            out << category; for (auto* field : fields) out << field;
        }
        for (const auto& [key, field] : m_fieldIdentifierMap) out << field->hoverDescription << field->visible;
        return out.str();
    }
    void Corrupt() {
        for (const auto& [key, field] : m_fieldIdentifierMap) {
            strcpy_s(field->buffer, field->bufsize, "changed"); field->QueueValue("");
            field->displayName = "callback label"; field->hoverDescription = "callback description";
            field->errored = true; field->visible = true; field->flags ^= EditorInput_Hex;
        }
        structureId = 77; popen = false; unsavedChanges = false; justAppliedChanges = true;
        setFocus = false; moveFocusAway = true; m_requestedClosure = true;
        m_windowTitle = "callback title"; m_categories.insert(99); m_categoryStringIdentifiers[99] = "changed";
        static_cast<FixtureEditor*>(m_editor)->Touch();
    }
    void OnUpdate(int, EditorInput*) override {
        ++updates;
        Check(m_editor->live_loadedMoveset == 0, "Single-form update retained live binding");
        if (failure == UpdateBytes) static_cast<FixtureEditor*>(m_editor)->Touch();
        if (failure == FirstUpdate || (failure == LateUpdate && updates == 2)) {
            Corrupt(); throw std::runtime_error("single-form update failed");
        }
    }
    void OnApply() override {
        ++applies;
        Check(m_editor->live_loadedMoveset == 0, "Single-form Apply retained live binding");
        for (auto* form : observers) form->RequestFieldUpdate(windowType, 1, 10, 12);
        if (failure == Reenter) Apply();
        if (failure == PoisonCommit) {
            try { m_editor->Mutate([&] { Corrupt(); throw std::runtime_error("caught callback child failure"); }); }
            catch (const std::exception&) {}
        }
        if (failure == FinalApply) { Corrupt(); throw std::runtime_error("single-form Apply failed"); }
        if (failure == UnknownApply) { Corrupt(); throw 42; }
    }
};
// Real move callbacks and private rename checkpoint, with inert editor storage and no OS window.
class MoveForm : public TEditorMove {
public:
    bool failTitle = false;
    int titles = 0;
    explicit MoveForm(FixtureEditor& editor) {
        m_editor = &editor; m_windowTypeName = "move"; windowType = TEditorWindowType_Move;
        m_fieldIdentifierMap.Own("move_name", Input("old"));
        m_fieldIdentifierMap.Own("other", Input("ok"));
        for (const auto& [name, field] : m_fieldIdentifierMap) {
            field->name = name.str(); m_fieldsCategoryMap[0].push_back(field);
        }
    }
    using EditorForm::Apply;
    auto Field(const char* name) { return m_fieldIdentifierMap.at(name); }
    auto Title() const { return m_windowTitle; }
    void ApplyWindowName() override {
        ++titles; m_windowTitle = "move title";
        if (failTitle) throw std::runtime_error("move title callback failed");
    }
};
class CreatedSubwindow final : public EditorSubwindow {
public:
    inline static int alive = 0;
    CreatedSubwindow() { ++alive; }
    ~CreatedSubwindow() override { --alive; }
    void Refresh(ImGuiID dock) override { nextDockId = dock; }
    bool Render() override { return false; }
};
class HeadlessOwner : public EditorVisuals {
public:
    enum Failure { None, Allocate, Initialize, Publish, FinalCallback, Unknown, Poison, Reenter } failure = None;
    int navigation = 7;
    std::vector<DisplayableMove*> filtered;
    HeadlessOwner(GameProcess& process, GameData& game, const GameInfo* info);
    auto& Editor() { return *static_cast<FixtureEditor*>(m_abstractEditor.get()); }
    auto Forms() const { return m_structWindows; }
    auto Count() const { return m_structWindows.size(); }
    auto Signature() const {
        std::ostringstream out; out << navigation;
        for (auto* row : filtered) out << row;
        for (auto* form : m_structWindows) out << form;
        for (auto* window : m_subwindows) out << window;
        return out.str();
    }
    EditorForm* Adopt(std::unique_ptr<EditorForm> form) {
        auto* result = form.get(); PublishFormWindow(std::move(form)); return result;
    }
    void PublishFormWindow(std::unique_ptr<EditorForm> form) override {
        EditorVisuals::PublishFormWindow(std::move(form));
        if (failure == Publish) throw std::bad_alloc();
    }
    struct ViewState final : EditorForm::TransactionState {
        std::unique_ptr<EditorForm::TransactionState> base;
        HeadlessOwner& owner;
        int navigation;
        std::vector<DisplayableMove*> filtered;
        bool committed = false;
        explicit ViewState(HeadlessOwner& view) : base(view.EditorVisuals::BeginWindowTransaction()), owner(view),
            navigation(view.navigation), filtered(view.filtered) {}
        ~ViewState() override { if (!committed) { std::swap(navigation, owner.navigation); filtered.swap(owner.filtered); } }
        void Commit() noexcept override { base->Commit(); committed = true; }
    };
    std::unique_ptr<EditorForm::TransactionState> BeginWindowTransaction() override { return std::make_unique<ViewState>(*this); }
    void PopulateWindowCreatorMap() override {}
    void OnKeyboardShortcut(const std::string&) override {}
    void RenderGameSpecific(int) override {}
    void ClearFactory() {
        for (auto& [type, creator] : m_windowCreatorMap) delete creator.allocator;
        m_windowCreatorMap.clear();
    }
};
class CreatedForm final : public EditorForm {
public:
    inline static int alive = 0;
    CreatedForm(const std::string&, EditorWindowType type, uint16_t id, EditorLogic* editor, EditorVisuals* owner, int, const char*) {
        auto* host = static_cast<HeadlessOwner*>(owner);
        if (host->failure == HeadlessOwner::Allocate) throw std::bad_alloc();
        m_editor = editor; m_baseWindow = owner; structureId = id; windowType = type;
        m_fieldIdentifierMap.Own("field", Input("new"));
        ++alive;
    }
    ~CreatedForm() override { --alive; }
    void OnInitEnd() override {
        auto* host = static_cast<HeadlessOwner*>(m_baseWindow);
        host->navigation = 99;
        if (host->failure == HeadlessOwner::Initialize) throw std::runtime_error("new form initialization failed");
    }
};
HeadlessOwner::HeadlessOwner(GameProcess& process, GameData& game, const GameInfo* info) {
    m_abstractEditor = std::make_unique<FixtureEditor>(process, game, info);
    AddSubwindow(new CreatedSubwindow);
    m_windowCreatorMap.emplace(TEditorWindowType_CancelExtradata, WindowCreator{new EditorWindowFactory<CreatedForm>, "test"});
    auto row = std::make_unique<DisplayableMove>(); row->name = "original"; row->flags = 0;
    filtered.push_back(row.get()); Editor().displayableMovelist.push_back(row.release());
}
class StructuralForm : public SingleForm {
public:
    explicit StructuralForm(HeadlessOwner& host) : SingleForm(host.Editor()) {
        m_baseWindow = &host; structureId = 10; windowType = TEditorWindowType_CancelExtradata;
    }
    using EditorForm::DuplicateStructure;
    using EditorForm::DeleteStructure;
    using EditorForm::TryDuplicateStructure;
    using EditorForm::TryDeleteStructure;
    int prepared = 0, duplicated = 0, deleted = 0;
    bool sawClosed = false;
    std::unique_ptr<TransactionState> BeginEditTransaction() override { return EditorForm::BeginEditTransaction(); }
    void SetDeletionPopup(bool open) { m_deletionPopupOpen = open; }
    auto Popup() const { return m_deletionPopupOpen; }
    void SetDeletable(bool value) { m_isDeletable = value; }
    void PrepareDuplication(unsigned int, unsigned int amount) override {
        Check(amount == 0, "Single-form duplication changed callback list-size convention");
        ++prepared;
        auto* host = static_cast<HeadlessOwner*>(m_baseWindow);
        Check(m_editor->live_loadedMoveset == 0, "Duplication preparation retained live binding");
        host->navigation = 88; host->filtered = host->Editor().displayableMovelist;
    }
    void FailIfRequested() {
        auto* host = static_cast<HeadlessOwner*>(m_baseWindow);
        if (host->failure == HeadlessOwner::Reenter) DuplicateStructure();
        if (host->failure == HeadlessOwner::Poison) {
            try { m_editor->Mutate([&] { Corrupt(); host->AddSubwindow(new CreatedSubwindow); throw std::runtime_error("caught structural child failure"); }); }
            catch (const std::exception&) {}
        }
        if (host->failure == HeadlessOwner::FinalCallback || host->failure == HeadlessOwner::Unknown) {
            Corrupt(); host->AddSubwindow(new CreatedSubwindow); host->navigation = 66; host->filtered = host->Editor().displayableMovelist;
            if (host->failure == HeadlessOwner::Unknown) throw 42;
            throw std::runtime_error("late structural callback failed");
        }
    }
    void OnDuplication(unsigned int, unsigned int) override { ++duplicated; sawClosed = !popen; FailIfRequested(); }
    void OnDeletion() override {
        ++deleted; Check(m_editor->live_loadedMoveset == 0, "Deletion callback retained live binding");
        EditorForm::OnDeletion(); FailIfRequested();
    }
};
class List : public EditorFormList {
public:
    bool failResize = false, failDetails = false;
    int updates = 0;
    enum Failure { None, Resize, Details, Reorder, Update, ApplyResize, FinalApply, Reenter, UnknownResize, UnknownApply } failure = None;
    std::vector<EditorForm*> observers;
    void Corrupt() {
        for (auto& row : m_items) {
            row->itemLabel = "callback label"; row->color = 123; row->id = -77;
            row->openStatus = EditorFormTreeview_ForceClose;
            for (const auto& [key, field] : row->identifierMap) {
                strcpy_s(field->buffer, field->bufsize, "edited");
                field->QueueValue(""); field->flags ^= EditorInput_Hex;
                field->displayName = "callback field"; field->hoverDescription = "callback hover";
                field->errored = true; field->visible = true;
            }
        }
        m_categories.insert(42); m_categoryStringIdentifiers[42] = "callback category";
        m_deletedItemIds.insert(999); m_resizeEventKey = "callback key";
        m_windowTitle = "callback title"; m_requestedClosure = true;
        structureId = 77; popen = false; justAppliedChanges = true; unsavedChanges = false;
        moveFocusAway = true; setFocus = false; m_listSize = 999; m_listSizeChange = 42;
        static_cast<FixtureEditor*>(m_editor)->Touch();
    }
    std::string Signature() const {
        std::ostringstream out;
        out << m_listSize << ':' << m_listSizeChange << ':' << structureId << ':' << m_resizeEventKey
            << ':' << m_windowTitle << ':' << m_requestedClosure << popen << justAppliedChanges
            << unsavedChanges << moveFocusAway << setFocus;
        for (auto category : m_categories) out << ':' << int(category);
        for (const auto& [category, label] : m_categoryStringIdentifiers) out << ':' << category << label;
        for (auto id : m_deletedItemIds) out << ':' << id;
        for (const auto& row : m_items) {
            out << ':' << row.get() << ':' << row->id << ':' << row->itemLabel << ':' << row->color << ':' << row->openStatus;
            for (const auto& [category, fields] : row->categoryMap) {
                out << ':' << category;
                for (auto* field : fields) out << ':' << field;
            }
            for (const auto& [key, field] : row->identifierMap)
                out << ':' << field << ':' << static_cast<void*>(field->buffer) << ':' << field->buffer
                    << ':' << field->flags << field->errored << field->visible << field->displayName << field->hoverDescription
                    << bool(field->nextValue) << std::quoted(field->nextValue.value_or(""));
        }
        return out.str();
    }
    auto Deleted() const { return m_deletedItemIds; }
    using EditorFormList::RemoveItem;
    using EditorFormList::MoveItem;
    using EditorFormList::DeleteStructure;
    using EditorForm::DuplicateStructure;
    int openedListSize = -1;
    void OpenDuplicatedForm(uint32_t, int size) override { openedListSize = size; }
    using EditorForm::TryApply;
    using EditorFormList::TryListEdit;
    auto Error() const { return m_operationError; }
    std::unique_ptr<TransactionState> BeginListTransaction() override {
        std::vector<EditorForm*> forms = observers;
        forms.push_back(this); forms.push_back(this); // Exercise batch de-duplication.
        return EditorVisuals::BeginFormTransaction(forms);
    }
    List(FixtureEditor& editor) {
        m_editor = &editor; m_windowTypeName = "test"; structureId = 10;
        for (int i = 0; i < 2; ++i) {
            auto row = std::make_unique<FieldItem>();
            row->id = 10 + i; row->identifierMap.Own("b", Input(i ? "end" : "old"));
            row->identifierMap.Own("a", Input("0"));
            for (auto& [key, field] : row->identifierMap) row->categoryMap[0].push_back(field);
            row->categoryMap[99] = {};
            m_items.push_back(std::move(row));
        }
        m_listSize = m_items.size();
    }
    using EditorFormList::PasteText;
    using EditorFormList::Apply;
    using EditorFormList::CreateNewItem;
    using EditorFormList::DuplicateItemToEnd;
    using EditorFormList::CanDuplicateItemToEnd;
    auto Row(size_t i) { return m_items.at(i).get(); }
    auto Size() { return m_listSize; }
    auto Change() { return m_listSizeChange; }
    void OnUpdate(int, EditorInput*) override {
        ++updates;
        Check(m_editor->live_loadedMoveset == 0, "Pending Apply callback retained a live binding");
        if (failure == Update && updates == 2) { Corrupt(); throw std::runtime_error("late update failure"); }
    }
    void OnApplyResize(int change, int oldSize) override {
        for (auto* form : observers) form->RequestFieldUpdate(windowType, change, structureId, structureId + oldSize);
        if (failure == ApplyResize) { Corrupt(); throw std::runtime_error("late Apply resize failure"); }
    }
    void OnApply() override {
        if (failure == UnknownApply) { Corrupt(); throw 42; }
        if (failure == FinalApply) { Corrupt(); throw std::runtime_error("final Apply failure"); }
    }
    void OnReorder() override {
        Check(m_editor->live_loadedMoveset == 0, "Reorder callback retained a live binding");
        if (failure == Reorder) { Corrupt(); throw std::runtime_error("late reorder failure"); }
    }
    void OnResize() override {
        Check(m_editor->live_loadedMoveset == 0, "Resize callback retained a live binding");
        if (failure == UnknownResize) { Corrupt(); throw 42; }
        if (failure == Reenter) CreateNewItem();
        if (failResize || failure == Resize) { Corrupt(); throw std::runtime_error("resize failure"); }
    }
    void BuildItemDetails(int i) override {
        m_items[i]->itemLabel = "prepared";
        if ((failDetails || failure == Details) && i == 2) { Corrupt(); throw std::runtime_error("late label failure"); }
    }
};
}
void RunEditorFormTests() {
    static_assert(!std::is_copy_constructible_v<InputMap> && std::is_nothrow_move_constructible_v<InputMap>);
    struct Context {
        ImGuiContext* context = ImGui::CreateContext();
        Context() {
            auto& io = ImGui::GetIO(); io.IniFilename = nullptr; io.LogFilename = nullptr;
            io.DisplaySize = ImVec2(640, 480); io.DeltaTime = 1.0f / 60;
            unsigned char* pixels; int w, h; io.Fonts->GetTexDataAsRGBA32(&pixels, &w, &h);
            ImGui::NewFrame();
        }
        ~Context() { ImGui::EndFrame(); ImGui::DestroyContext(context); }
    } context;
    {
        InputMap fields; VectorSet<std::string> order;
        auto* original = EditorUtils::CreateField("value", order, fields, 0, EditorInput_U32, 42u);
        Reject([&] { EditorUtils::CreateField("value", order, fields, 0, EditorInput_U32, 7u); });
        Reject([&] { EditorUtils::CreateField("too-small", order, fields, 0, EditorInput_U32, 123u, 2); });
        Check(fields.size() == 1 && fields.at("value") == original && order.size() == 1, "Failed CreateField changed ownership or draw order");
        Reject([&] { fields.Own("value", Input("other")); });
        Reject([&] { fields.Own("null", OwnedEditorInput{}); });
        Reject([&] { (void)fields["missing"]; });
        Check(fields.size() == 1 && fields.at("value") == original, "Rejected ownership change modified fields");
        InputMap moved(std::move(fields));
        Check(fields.empty() && moved.at("value") == original, "Move construction duplicated field ownership");
        fields.Own("replaced", Input("old"));
        fields = std::move(moved);
        fields = std::move(fields);
        Check(moved.empty() && fields.at("value") == original, "Move assignment lost field ownership");
        fields.clear(); fields.clear();
    }
    const std::vector<size_t> widths{8, 8};
    auto decode = [&](std::string_view text) { return EditorClipboard::Decode(text, "test", widths, 3); };
    Reject([] { EditorClipboard::Text(nullptr); });
    for (auto text : {"", "test", "test\n", "tes\nx\ny\n", "test-more\nx\ny\n", "test\nx\n", "test\n12345678\ny\n", "test\nx\ny\nz\n"})
        Reject([&] { decode(text); });
    Reject([&] { decode(std::string("test\nx\n\0\n", 10)); });
    Reject([&] { decode(std::string(EditorClipboard::MaxBytes + 1, 'x')); });
    std::string huge(EditorClipboard::MaxBytes + 1, 'x');
    Reject([&] { EditorClipboard::Text(huge.c_str()); });
    std::string many = "test\n"; many.append(EditorClipboard::MaxFields + 1, '\n');
    const std::vector<size_t> one{8};
    Reject([&] { EditorClipboard::Decode(many, "test", one, EditorClipboard::MaxFields + 2); });
    Reject([&] { decode("test\na\nb\nc\nd\ne\nf\ng\nh\n"); });
    const std::vector<size_t> zero{0};
    Reject([&] { EditorClipboard::Decode("test\nx\n", "test", zero, 1); });
    Reject([&] { EditorClipboard::Decode("test\nx\n", "test", one, 0); });
    std::string maximum = "test\n"; maximum.append(EditorClipboard::MaxFields, '\n');
    Check(EditorClipboard::Decode(maximum, "test", one, EditorClipboard::MaxFields).size() == EditorClipboard::MaxFields,
        "Exact field budget was rejected");
    Check(decode("test\r\nx\r\ny\r\n")[0][1] == "y", "CRLF decoding failed");
    Check(decode("test\nx\ny")[0][1] == "y", "Final non-newline field lost");
    Check(decode("test\n\n\n")[0][1].empty(), "Empty fields lost");
    GameProcess process; GameData game; GameInfo info{}; info.dataString = info.minorDataString = "t7";
    FixtureEditor editor(process, game, &info);
    for (auto mode : {FixtureEditor::Empty, FixtureEditor::MissingOrder, FixtureEditor::NullField,
                      FixtureEditor::Unterminated, FixtureEditor::LateFactoryFailure}) {
        editor.mode = mode;
        Reject([&] { EditorForm form("test", EditorWindowType_INVALID, 0, &editor, nullptr, 0, "test"); });
        Reject([&] { EditorFormList list("test", EditorWindowType_INVALID, 10, &editor, nullptr, 2, "test"); });
    }
    editor.mode = FixtureEditor::LateRowFailure;
    Reject([&] { EditorFormList list("test", EditorWindowType_INVALID, 10, &editor, nullptr, 2, "test"); });
    editor.mode = FixtureEditor::WrongCount;
    Reject([&] { EditorFormList list("test", EditorWindowType_INVALID, 10, &editor, nullptr, 2, "test"); });
    editor.mode = FixtureEditor::Normal;
    Reject([&] { EditorFormList list("test", EditorWindowType_INVALID, 10, &editor, nullptr, -1, "test"); });
    Reject([&] { EditorForm form("test", EditorWindowType_INVALID, 0, nullptr, nullptr, 0, "test"); });
    {
        EditorForm form("test", EditorWindowType_INVALID, 0, &editor, nullptr, 0, "test");
        EditorFormList list("test", EditorWindowType_INVALID, 10, &editor, nullptr, 2, "test");
        list.OnInitEnd();
    }
    {
        Form form(editor);
        form.PasteText("test\nx\n\n");
        Reject([&] { form.PasteText("test\nnew\n12345678\n"); });
        Check(form.Field("b")->nextValue == "x" && form.Field("a")->nextValue == "", "Rejection changed pending fields");
        form.Apply();
        Check(editor.saved == std::vector<std::string>{"x", ""} && form.updates == 2 && !form.unsavedChanges, "Apply lost hidden pending values");
        form.Field("b")->QueueValue("bad");
        form.Apply();
        Check(editor.saves == 1 && form.unsavedChanges && form.Field("b")->errored, "Invalid pending value saved");
        auto field = form.Field("a");
        field->QueueValue(""); Check(field->ConsumeNextValue() && !field->ConsumeNextValue(), "Empty queue not consumed exactly once");
        Reject([&] { field->QueueValue("12345678"); });
        Reject([&] { field->QueueValue(std::string("x\0y", 3)); });
        Check(!field->nextValue && field->buffer[0] == 0, "Rejected field paste changed input");
    }
    {
        List list(editor); auto first = list.Row(0);
        list.Row(1)->identifierMap.at("b")->bufsize = 7;
        Reject([&] { list.PasteText("test\na\nb\nc\nd\n"); });
        Check(list.Row(0) == first && !first->identifierMap.at("b")->nextValue, "Destination schema rejection was not atomic");
    }
    {
        List list(editor); auto first = list.Row(0);
        // The budget must be rejected before CloneRow reads the deliberately small backing buffer.
        first->identifierMap.at("b")->bufsize = 64 * 1024 * 1024 + 1;
        Reject([&] { list.PasteText("test\na\nb\n"); });
        Check(list.Row(0) == first && list.Size() == 2, "Working-budget rejection changed list");
        first->identifierMap.at("b")->bufsize = 8;
    }
    for (bool late : {false, true}) {
        List list(editor); auto first = list.Row(0); auto last = list.Row(1);
        editor.live_loadedMoveset = 1234;
        list.failResize = !late; list.failDetails = late;
        Reject([&] { list.PasteText("test\na\nb\nc\nd\ne\nf\n"); });
        Check(list.Size() == 2 && list.Change() == 0 && list.Row(0) == first && list.Row(1) == last &&
            editor.live_loadedMoveset == 1234 && !list.unsavedChanges && !first->identifierMap.at("b")->nextValue,
            "Failed list paste did not roll back identity, count, pending values and live binding");
    }
    {
        List list(editor);
        list.PasteText("test\na\nb\nc\nd\ne\nf");
        Check(list.Size() == 3 && list.Change() == 1 && list.Row(1)->id == -1 && list.Row(2)->id == 11,
            "List growth did not retain terminator identity");
        for (size_t r = 0; r < list.Size(); ++r)
            Check(list.Row(r)->categoryMap.at(0)[0] == list.Row(r)->identifierMap.at("b"), "Cloned category points to old field");
        auto first = list.Row(0);
        Reject([&] { list.PasteText("test\nx\ny\nz"); });
        Check(list.Row(0) == first && list.Size() == 3, "Malformed list paste changed rows");
        // Flush through Apply's validation gate without running a list mutation on the synthetic byte store.
        list.Row(0)->identifierMap.at("b")->QueueValue("bad");
        list.Apply();
        Check(list.updates == 5 && list.Row(2)->identifierMap.at("a")->buffer == std::string("f"), "List Apply lost hidden pending fields");
    }
    {
        List list(editor); auto first = list.Row(0); auto last = list.Row(1);
        editor.live_loadedMoveset = 1234;
        Reject([&] { list.CreateNewItem(2); });
        list.failResize = true;
        Reject([&] { list.CreateNewItem(); });
        Check(list.Size() == 2 && list.Change() == 0 && list.Row(0) == first && list.Row(1) == last &&
            editor.live_loadedMoveset == 1234 && !list.unsavedChanges, "New-row failure changed list state");
        list.failResize = false;
        Check(list.CreateNewItem() == 1 && list.Size() == 3 && list.Row(1)->id == -1 && list.Row(2) == last,
            "New-row insertion lost terminator");
    }
    {
        // Row duplication appends a copy just before the end row; the rows before it keep their positions.
        List list(editor); auto first = list.Row(0); auto last = list.Row(1);
        Check(list.CanDuplicateItemToEnd(0) && !list.CanDuplicateItemToEnd(1) && !list.CanDuplicateItemToEnd(2), "Row duplication availability differs");
        Reject([&] { list.DuplicateItemToEnd(1); });
        Reject([&] { list.DuplicateItemToEnd(5); });
        Check(list.Size() == 2 && list.Change() == 0 && !list.unsavedChanges, "Rejected row duplication changed the list");
        strcpy_s(first->identifierMap.at("b")->buffer, first->identifierMap.at("b")->bufsize, "edited");
        Check(list.DuplicateItemToEnd(0) == 1 && list.Size() == 3 && list.Change() == 1 && list.unsavedChanges, "Row duplication result differs");
        auto copy = list.Row(1);
        Check(list.Row(0) == first && copy != first && copy->id == -1 && list.Row(2) == last &&
            std::string(copy->identifierMap.at("b")->buffer) == "edited" && copy->identifierMap.at("b") != first->identifierMap.at("b"),
            "Row duplication did not copy the row's current values before the end row");
        Check(list.DuplicateItemToEnd(0) == 2 && list.Row(1) == copy && list.Row(3) == last && list.Size() == 4, "Second row duplication was not appended");
        // The shared starting lists keep their first row fixed, as with the list buttons.
        List start(editor); start.structureId = 1;
        Check(!start.CanDuplicateItemToEnd(0), "Starting list row 0 can be duplicated");
    }
    {
        // The + button copies the form row itself: after unapplied inserts a form index lies past the stored list,
        // and reading the editor at structureId + index would read beyond its table.
        List list(editor); auto last = list.Row(1);
        editor.failNewRow = true; // The editor's single-row factory must not be consulted
        list.Row(0)->identifierMap.at("b")->QueueValue("queued");
        Check(list.CreateNewItem(0) == 0 && list.CreateNewItem(2) == 2 && list.Size() == 4 && list.Change() == 2,
            "Row insertion past the stored list failed");
        auto copy = list.Row(0); auto copyPastEnd = list.Row(2);
        Check(copy->id == -1 && copyPastEnd->id == -1 && list.Row(3) == last && list.Row(1)->id == 10,
            "Inserted rows lost their identities");
        Check(copy->identifierMap.at("b") != list.Row(1)->identifierMap.at("b") &&
            copy->identifierMap.at("b")->buffer != list.Row(1)->identifierMap.at("b")->buffer &&
            copy->identifierMap.at("b")->nextValue == "queued" && copyPastEnd->identifierMap.at("b")->buffer == std::string("end"),
            "New row was not prefilled from the copied form row");
        Check(copy->categoryMap.at(0)[0] == copy->identifierMap.at("b") && copyPastEnd->categoryMap.at(0)[0] == copyPastEnd->identifierMap.at("b"),
            "New row categories point to the copied row's fields");
        editor.failNewRow = false;
    }
    {
        List list(editor);
        list.PasteText("test\na\nb\n");
        Check(list.Size() == 2 && list.Row(1)->id == 11 && !list.Row(1)->identifierMap.at("b")->nextValue,
            "Short paste changed retained trailing row");
    }
    // Every rejected operation must restore values, pointer identity, metadata and editor storage.
    auto unchanged = [&](List& list, auto&& action) {
        const auto before = list.Signature();
        const auto bytes = editor.ByteValue(); const auto pointer = editor.BytePointer();
        editor.live_loadedMoveset = 1234;
        Reject(action);
        Check(list.Signature() == before && editor.ByteValue() == bytes && editor.BytePointer() == pointer &&
            editor.live_loadedMoveset == 1234, "Callback failure left partial list/form/editor state");
    };
    {
        List list(editor); editor.failAllocation = true;
        unchanged(list, [&] { list.CreateNewItem(); });
        unchanged(list, [&] { list.RemoveItem(0); });
        editor.failAllocation = false;
    }
    for (auto failure : {List::Resize, List::Reenter}) {
        List list(editor); list.failure = failure;
        unchanged(list, [&] { list.CreateNewItem(); });
        unchanged(list, [&] { list.RemoveItem(0); });
    }
    {
        List list(editor); list.CreateNewItem(); list.failure = List::Reorder;
        unchanged(list, [&] { list.MoveItem(0, 1); });
        list.failure = List::Details;
        unchanged(list, [&] { list.PasteText("test\na\nb\nc\nd\ne\nf\ng\nh\n"); });
        list.failure = List::None;
        auto first = list.Row(0); auto last = list.Row(list.Size() - 1);
        list.MoveItem(0, 1);
        Check(list.Row(1) == first && list.Row(list.Size() - 1) == last && editor.live_loadedMoveset == 0,
            "Successful reorder lost retained row identity or live invalidation");
        unchanged(list, [&] { list.MoveItem(0, list.Size() - 1); });
        unchanged(list, [&] { list.RemoveItem(list.Size() - 1); });
    }
    {
        List list(editor); list.PasteText("test\na\nb\nc\nd\n"); list.failure = List::Update;
        unchanged(list, [&] { list.Apply(); });
    }
    for (auto failure : {List::ApplyResize, List::FinalApply, List::None}) {
        List list(editor); list.CreateNewItem(); list.CreateNewItem(); list.RemoveItem(0);
        list.Row(0)->identifierMap.at("b")->QueueValue("");
        list.failure = failure; editor.writeSaves = true;
        if (failure == List::None) editor.failSaveAt = editor.saves + 2;
        unchanged(list, [&] { list.Apply(); });
        Check(list.Deleted().contains(10), "Failed Apply lost deletion bookkeeping");
        editor.failSaveAt = -1; editor.writeSaves = false;
        list.failure = List::None; list.Apply();
        Check(list.Change() == 0 && list.Deleted().empty() && list.justAppliedChanges && !list.unsavedChanges,
            "Apply retry did not commit normalized list state");
    }
    {
        List list(editor), observerList(editor); Form first(editor), late(editor);
        observerList.structureId = 50;
        list.observers = {&first, &observerList, &late};
        list.CreateNewItem(); late.failObservation = true;
        auto firstBefore = first.Signature(); auto lateBefore = late.Signature(); auto listBefore = observerList.Signature();
        editor.writeSaves = true;
        unchanged(list, [&] { list.Apply(); });
        Check(first.Signature() == firstBefore && late.Signature() == lateBefore && observerList.Signature() == listBefore,
            "Late observer failure did not restore earlier observer forms");
        editor.writeSaves = false;
        late.failObservation = false; list.Apply();
        Check(first.Signature() != firstBefore && observerList.structureId == 51 && !late.popen,
            "Successful observer updates were rolled back");
    }
    {
        List list(editor); list.failure = List::ApplyResize;
        unchanged(list, [&] { list.DeleteStructure(); });
        list.failure = List::None; list.DeleteStructure();
        Check(!list.popen && !list.unsavedChanges, "Successful whole-list deletion remained open");
    }

    {
        List list(editor); Form first(editor), invalid(editor);
        list.observers = {&first, &invalid};
        auto firstBefore = first.Signature();
        invalid.Field("b")->bufsize = 64 * 1024 * 1024 + 1;
        unchanged(list, [&] { list.RemoveItem(0); });
        Check(first.Signature() == firstBefore, "Checkpoint failure did not restore previously staged forms");
        invalid.Field("b")->bufsize = 8;
    }
    {
        List list(editor); auto first = list.Row(0); auto last = list.Row(1);
        list.CreateNewItem(); list.RemoveItem(1);
        Check(list.Size() == 2 && list.Change() == 0 && list.Deleted().empty() && list.Row(0) == first && list.Row(1) == last,
            "Deleting an unsaved new row changed persisted deletion IDs or retained rows");
        Check(list.Row(0)->categoryMap.contains(99), "Transaction dropped an empty category");
    }
    {
        List list(editor); list.failure = List::FinalApply;
        const auto before = list.Signature(); const auto bytes = editor.ByteValue();
        editor.live_loadedMoveset = 1234;
        list.TryApply();
        Check(list.Signature() == before && editor.ByteValue() == bytes && editor.live_loadedMoveset == 1234 && !list.Error().empty(),
            "UI error reporting changed the rolled-back edit state");
    }

    {
        List list(editor); list.RemoveItem(0); list.RemoveItem(0);
        Check(list.Size() == 0 && list.Change() == -2, "Singleton deletion did not prepare an empty list");
        list.failure = List::FinalApply;
        unchanged(list, [&] { list.Apply(); });
        list.failure = List::None; list.Apply();
        Check(!list.popen && list.Deleted().empty() && list.Change() == 0, "Empty-list Apply did not commit closure");
    }
    for (auto failure : {List::UnknownResize, List::UnknownApply}) {
        List list(editor); list.failure = failure;
        const auto before = list.Signature(); const auto bytes = editor.ByteValue();
        editor.live_loadedMoveset = 1234;
        if (failure == List::UnknownResize) list.TryListEdit([&] { list.RemoveItem(0); });
        else list.TryApply();
        Check(list.Signature() == before && editor.ByteValue() == bytes && editor.live_loadedMoveset == 1234 && !list.Error().empty(),
            "Non-standard callback exception escaped UI handling or left partial state");
    }

    auto singleUnchanged = [&](SingleForm& form, auto&& action) {
        const auto before = form.Signature(); const auto bytes = editor.ByteValue(); const auto pointer = editor.BytePointer();
        editor.live_loadedMoveset = 1234;
        Reject(action);
        Check(form.Signature() == before && editor.ByteValue() == bytes && editor.BytePointer() == pointer &&
            editor.live_loadedMoveset == 1234, "Single-form failure left partial UI/editor state");
    };
    for (auto failure : {SingleForm::FirstUpdate, SingleForm::LateUpdate, SingleForm::FinalApply, SingleForm::Reenter, SingleForm::PoisonCommit}) {
        SingleForm form(editor); form.Field("b")->QueueValue("queued"); form.Field("a")->QueueValue("");
        form.failure = failure; editor.writeSaves = true;
        singleUnchanged(form, [&] { form.Apply(); });
        form.failure = SingleForm::None; form.Apply();
        Check(!form.unsavedChanges && form.justAppliedChanges && form.Field("a")->buffer[0] == 0 && editor.live_loadedMoveset == 0,
            "Single-form retry lost empty/hidden pending values or failed to finalize");
        editor.writeSaves = false;
    }
    for (int stage = 0; stage < 3; ++stage) {
        SingleForm form(editor); form.Field("b")->QueueValue("queued"); form.Field("a")->QueueValue("");
        editor.failAllocation = stage == 0;
        if (stage == 1) editor.failValidationAt = editor.validations + 2;
        if (stage == 2) { editor.writeSaves = true; editor.failSaveAt = editor.saves + 1; }
        singleUnchanged(form, [&] { form.Apply(); });
        editor.failAllocation = false; editor.failValidationAt = -1; editor.failSaveAt = -1; editor.writeSaves = false;
    }
    {
        SingleForm form(editor); Form first(editor), late(editor); List peer(editor); peer.structureId = 50;
        form.observers = {&first, &peer, &late}; late.failObservation = true;
        form.Field("b")->QueueValue("queued");
        auto firstBefore = first.Signature(); auto lateBefore = late.Signature(); auto peerBefore = peer.Signature();
        editor.writeSaves = true;
        singleUnchanged(form, [&] { form.Apply(); });
        Check(first.Signature() == firstBefore && late.Signature() == lateBefore && peer.Signature() == peerBefore,
            "Single-form callback failure left partial observer state");
        late.failObservation = false; form.Apply(); editor.writeSaves = false;
        Check(first.Signature() != firstBefore && peer.structureId == 51, "Successful single-form observers were rolled back");
    }
    {
        SingleForm form(editor); Form invalid(editor); form.observers = {&invalid};
        invalid.Field("b")->bufsize = 64 * 1024 * 1024 + 1;
        singleUnchanged(form, [&] { form.Apply(); });
        invalid.Field("b")->bufsize = 8;
    }
    {
        SingleForm form(editor); form.failure = SingleForm::UpdateBytes;
        const auto bytes = editor.ByteValue(); const auto saves = editor.saves;
        form.Field("b")->QueueValue("bad"); form.Field("a")->QueueValue(""); form.Apply();
        Check(editor.ByteValue() == bytes && editor.saves == saves && form.applies == 0 && form.unsavedChanges &&
            form.Field("b")->errored && !form.Field("b")->nextValue && form.Field("a")->buffer[0] == 0,
            "Invalid single-form Apply saved data, ran OnApply or lost validation state");
    }
    for (auto failure : {SingleForm::FinalApply, SingleForm::UnknownApply}) {
        SingleForm form(editor); form.failure = failure;
        const auto before = form.Signature(); const auto bytes = editor.ByteValue();
        editor.live_loadedMoveset = 1234; form.TryApply();
        Check(form.Signature() == before && editor.ByteValue() == bytes && editor.live_loadedMoveset == 1234 && !form.Error().empty(),
            "Single-form UI error handling changed rolled-back state");
    }
    for (int stage = 0; stage < 4; ++stage) {
        FixtureEditor moveEditor(process, game, &info);
        auto row = std::make_unique<DisplayableMove>(); row->name = "original"; row->flags = 0;
        auto* originalRow = row.get(); moveEditor.displayableMovelist.push_back(row.release());
        MoveForm form(moveEditor); auto* field = form.Field("move_name"); auto* buffer = field->buffer;
        field->QueueValue("new"); const auto bytes = moveEditor.ByteValue(); const auto pointer = moveEditor.BytePointer();
        moveEditor.live_loadedMoveset = 1234; moveEditor.writeSaves = true;
        if (stage == 0) moveEditor.failSaveAt = 1;
        if (stage == 1) moveEditor.failRecompute = true;
        if (stage == 2) form.failTitle = true;
        if (stage == 3) moveEditor.poisonRecompute = true;
        Reject([&] { form.Apply(); });
        Check(form.Field("move_name") == field && field->buffer == buffer && field->nextValue == "new" &&
            std::string(field->buffer) == "old" && form.Title().empty() && moveEditor.ByteValue() == bytes &&
            moveEditor.BytePointer() == pointer && moveEditor.displayableMovelist[0] == originalRow && originalRow->name == "original" &&
            originalRow->flags == 0 && !moveEditor.mustReloadMovelist && moveEditor.live_loadedMoveset == 1234,
            "Real move Apply failure lost fields, cache identity or editor bytes");
        const auto titles = form.titles;
        field->nextValue.reset(); moveEditor.failSaveAt = -1; moveEditor.failRecompute = false;
        moveEditor.poisonRecompute = false; form.failTitle = false; form.Apply();
        Check(form.titles == titles && moveEditor.mustReloadMovelist, "Move rollback retained a false rename or missed deferred cache refresh");
    }
    {
        FixtureEditor moveEditor(process, game, &info);
        auto row = std::make_unique<DisplayableMove>(); row->name = "original"; row->flags = 0;
        auto* originalRow = row.get(); moveEditor.displayableMovelist.push_back(row.release());
        MoveForm form(moveEditor); form.Field("move_name")->QueueValue("new"); form.Field("other")->QueueValue("bad"); form.Apply();
        Check(moveEditor.saves == 0 && form.unsavedChanges && form.titles == 0, "Invalid move Apply finalized rename");
        form.Field("other")->QueueValue("ok"); moveEditor.writeSaves = true; moveEditor.poisonRecompute = true;
        Reject([&] { form.Apply(); });
        Check(form.titles == 1 && moveEditor.displayableMovelist[0] == originalRow, "Late move commit failure did not restore cache identity");
        moveEditor.poisonRecompute = false; form.Apply();
        Check(form.titles == 2 && !form.unsavedChanges && moveEditor.mustReloadMovelist, "Move rollback lost an existing pending rename");
        form.Apply(); Check(form.titles == 2, "Committed rename was not cleared");
    }
    {
        FixtureEditor moveEditor(process, game, &info);
        auto row = std::make_unique<DisplayableMove>(); row->name = "original"; row->flags = 0;
        auto* originalRow = row.get(); moveEditor.displayableMovelist.push_back(row.release());
        MoveForm form(moveEditor); const auto pointer = moveEditor.BytePointer(); form.Apply();
        Check(moveEditor.BytePointer() == pointer && moveEditor.displayableMovelist[0] == originalRow &&
            originalRow->name == "original" && !moveEditor.mustReloadMovelist && form.titles == 0,
            "No-op move Apply published working-cache pointers or metadata");
    }

    auto structuralUnchanged = [&](HeadlessOwner& host, StructuralForm& source, auto&& action) {
        const auto before = source.Signature(); const auto windows = host.Signature(); const auto popup = source.Popup();
        const auto bytes = host.Editor().ByteValue(); const auto pointer = host.Editor().BytePointer();
        auto cache = host.Editor().displayableMovelist;
        const auto alive = CreatedForm::alive; const auto subwindows = CreatedSubwindow::alive;
        host.Editor().live_loadedMoveset = 1234;
        Reject(action);
        Check(source.Signature() == before && source.Popup() == popup && host.Signature() == windows &&
            host.Editor().ByteValue() == bytes && host.Editor().BytePointer() == pointer &&
            host.Editor().displayableMovelist == cache && host.Editor().live_loadedMoveset == 1234 && CreatedForm::alive == alive &&
            CreatedSubwindow::alive == subwindows,
            "Structural failure left editor bytes, cache pointers, windows or source state changed");
    };
    for (auto stage : {HeadlessOwner::Allocate, HeadlessOwner::Initialize, HeadlessOwner::Publish,
                       HeadlessOwner::FinalCallback, HeadlessOwner::Poison, HeadlessOwner::Reenter}) {
        HeadlessOwner host(process, game, &info);
        auto* source = static_cast<StructuralForm*>(host.Adopt(std::make_unique<StructuralForm>(host)));
        source->Field("b")->QueueValue("bad"); source->Field("a")->QueueValue(""); source->unsavedChanges = true;
        host.failure = stage;
        structuralUnchanged(host, *source, [&] { source->DuplicateStructure(); });
        Check(host.Count() == 1, "Failed duplication published an extra form");
        host.failure = HeadlessOwner::None;
        Check(source->DuplicateStructure() == 22 && host.Editor().duplicatedAmount == 1 && host.Editor().saves == 0 &&
            host.Count() == 2 && source->justAppliedChanges && source->popen && host.Editor().live_loadedMoveset == 0 &&
            host.filtered == host.Editor().displayableMovelist &&
            source->Field("b")->nextValue == "bad", "Duplication retry lost scalar count, window publication or pending source edits");
    }
    for (int stage = 0; stage < 5; ++stage) {
        HeadlessOwner host(process, game, &info);
        auto* source = static_cast<StructuralForm*>(host.Adopt(std::make_unique<StructuralForm>(host)));
        if (stage == 0) host.Editor().failDuplicate = true;
        if (stage == 1) host.Editor().duplicateResult = 0;
        if (stage == 2) host.Editor().duplicateResult = 65536;
        if (stage == 3) host.Editor().failAllocation = true;
        if (stage == 4) host.ClearFactory();
        structuralUnchanged(host, *source, [&] { source->DuplicateStructure(); });
    }
    {
        HeadlessOwner host(process, game, &info);
        auto* source = static_cast<StructuralForm*>(host.Adopt(std::make_unique<StructuralForm>(host)));
        Check(host.OpenFormWindow(source->windowType, 22), "Cannot prepare an existing duplicate window");
        auto* existing = host.Forms().back(); existing->setFocus = false; source->popen = true;
        host.failure = HeadlessOwner::FinalCallback;
        structuralUnchanged(host, *source, [&] { source->DuplicateStructure(); });
        Check(!existing->setFocus && host.Count() == 2, "Failed duplication changed existing window focus");
        host.failure = HeadlessOwner::None; source->DuplicateStructure();
        Check(existing->setFocus && host.Count() == 2, "Existing duplicate window was not reused");
    }
    for (auto stage : {HeadlessOwner::FinalCallback, HeadlessOwner::Poison, HeadlessOwner::Reenter}) {
        HeadlessOwner host(process, game, &info);
        auto* source = static_cast<StructuralForm*>(host.Adopt(std::make_unique<StructuralForm>(host)));
        auto* observer = static_cast<Form*>(host.Adopt(std::make_unique<Form>(host.Editor())));
        source->SetDeletionPopup(true); source->unsavedChanges = true; source->Field("b")->QueueValue("queued");
        const auto observed = observer->Signature(); host.failure = stage;
        structuralUnchanged(host, *source, [&] { source->DeleteStructure(); });
        Check(observer->Signature() == observed, "Deletion failure left observer notifications applied");
        host.failure = HeadlessOwner::None; source->DeleteStructure();
        Check(!source->popen && !source->unsavedChanges && source->justAppliedChanges && !source->Popup() &&
            observer->Signature() != observed && host.Editor().saves == 0 && host.Editor().live_loadedMoveset == 0,
            "Successful deletion did not commit observers and closure without saving pending edits");
    }
    for (int stage = 0; stage < 4; ++stage) {
        HeadlessOwner host(process, game, &info);
        auto* source = static_cast<StructuralForm*>(host.Adopt(std::make_unique<StructuralForm>(host)));
        auto* first = static_cast<Form*>(host.Adopt(std::make_unique<Form>(host.Editor())));
        auto* late = static_cast<Form*>(host.Adopt(std::make_unique<Form>(host.Editor())));
        if (stage == 0) host.Editor().failDelete = true;
        if (stage == 1) late->failObservation = true;
        if (stage == 2) host.Editor().failAllocation = true;
        if (stage == 3) source->SetDeletable(false);
        const auto before = first->Signature(); const auto after = late->Signature();
        structuralUnchanged(host, *source, [&] { source->DeleteStructure(); });
        Check(first->Signature() == before && late->Signature() == after, "Late deletion notification changed earlier observers");
    }
    for (bool duplication : {false, true}) {
        HeadlessOwner host(process, game, &info);
        auto* source = static_cast<StructuralForm*>(host.Adopt(std::make_unique<StructuralForm>(host)));
        source->SetDeletionPopup(true); host.failure = HeadlessOwner::Unknown;
        const auto before = source->Signature(); const auto windows = host.Signature(); const auto bytes = host.Editor().ByteValue();
        host.Editor().live_loadedMoveset = 1234;
        Check(!(duplication ? source->TryDuplicateStructure() : source->TryDeleteStructure()) && !source->Error().empty() &&
            source->Signature() == before && source->Popup() && host.Signature() == windows &&
            host.Editor().ByteValue() == bytes && host.Editor().live_loadedMoveset == 1234,
            "Structural UI handler did not report and roll back a non-standard callback exception");
    }
    Check(CreatedForm::alive == 0, "Duplicated form ownership leaked");

    {
        HeadlessOwner host(process, game, &info);
        auto* source = static_cast<StructuralForm*>(host.Adopt(std::make_unique<StructuralForm>(host)));
        host.failure = HeadlessOwner::FinalCallback;
        structuralUnchanged(host, *source, [&] { source->DuplicateStructure(); });
        Check(source->sawClosed && source->popen, "Failed duplication did not restore the overwritten source window");
        host.failure = HeadlessOwner::None; source->DuplicateStructure();
        Check(!source->popen, "Successful duplication did not preserve normal window replacement behavior");
    }
    {
        List list(editor); list.CreateNewItem(); const auto saves = editor.saves;
        Check(list.DuplicateStructure() == 22 && editor.duplicatedAmount == 2 && list.openedListSize == 2 &&
            list.Size() == 3 && list.Change() == 1 && list.unsavedChanges && editor.saves == saves,
            "List duplication no longer uses the original persisted size independently of pending rows");
    }
    for (int stage = 0; stage < 3; ++stage) {
        HeadlessOwner host(process, game, &info);
        auto* source = static_cast<StructuralForm*>(host.Adopt(std::make_unique<StructuralForm>(host)));
        if (stage == 0) source->structureId = 0;
        if (stage == 1) source->uniqueType = true;
        if (stage == 2) source->SetDeletable(false);
        structuralUnchanged(host, *source, [&] { source->DeleteStructure(); });
        if (stage == 1) structuralUnchanged(host, *source, [&] { source->DuplicateStructure(); });
    }
    Check(CreatedForm::alive == 0, "Structural operation window ownership leaked");

    {
        HeadlessOwner host(process, game, &info);
        auto* source = static_cast<StructuralForm*>(host.Adopt(std::make_unique<StructuralForm>(host)));
        host.Editor().noStructuralChange = true;
        structuralUnchanged(host, *source, [&] { source->DeleteStructure(); });
        structuralUnchanged(host, *source, [&] { source->DuplicateStructure(); });
        Check(source->deleted == 0 && source->prepared == 0 && source->duplicated == 0,
            "Unsupported no-op structure mutation published callbacks or success");
    }

    Check(CreatedSubwindow::alive == 0, "Structural rollback or host destruction leaked a subwindow");

}
