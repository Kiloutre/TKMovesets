#include <ImGui.h>

#include <format>
#include <filesystem>

#include "Compression.hpp"
#include "Localization.hpp"
#include "imgui_extras.hpp"
#include "helpers.hpp"
#include "EditorVisuals.hpp"
#include "GameSharedMem.hpp"
#include "Keybinds.hpp"

// Structures

EditorVisuals::EditorVisuals() = default;

// -- Private methods -- //
EditorForm* EditorVisuals::AllocateFormWindow(EditorWindowType windowType, uint16_t id, int listSize)
{
	auto windowCreator = m_windowCreatorMap.find(windowType);
	if (windowCreator != m_windowCreatorMap.end()) {
		auto typeName = windowCreator->second.typeName;
		std::unique_ptr<EditorForm> newWin((EditorForm*)windowCreator->second.allocator->allocate(m_subwindowsTitle, windowType, id, m_abstractEditor.get(), this, listSize, typeName));
        if (!newWin) throw std::runtime_error("Editor form allocator returned no window.");
		newWin->OnInitEnd();
		return newWin.release();
	}

	DEBUG_LOG("ERROR: NO WINDOW FOUND FOR WINDOW TYPE %u\n", windowType);

	return nullptr;
}

bool EditorVisuals::OpenFormWindow(EditorWindowType windowType, uint16_t structId, int listSize)
{
	int availableOverwriteIndex = -1;
	for (int i = 0; i < m_structWindows.size(); ++i) {
		EditorForm* structWin = m_structWindows[i];
		if (structWin->windowType != windowType) {
			continue;
		}

		if (structWin->structureId == structId) {
			structWin->setFocus = true;
			// Prevent duplicate move window creation
			return true;
		}

		if ((structWin->unsavedChanges == false && structWin->currentViewport == m_viewport)
			|| structWin->uniqueType) {
			// Don't overwrite windows with unsaved changes OR that have been detached
			availableOverwriteIndex = i;
		}
	}

	bool openNew = ImGui::IsKeyDown(ImGuiKey_LeftCtrl);
	std::unique_ptr<EditorForm> newWin(AllocateFormWindow(windowType, structId, listSize));
	if (newWin != nullptr)
	{
		newWin->nextDockId = m_dockId;

        PublishFormWindow(std::move(newWin));

		if (!openNew && availableOverwriteIndex != -1) {
			// Can't delete here because OpenFormWindow can be called by EditorForm classes
			// Close the window and let deletion occur during next rendering
			m_structWindows[availableOverwriteIndex]->popen = false;
		}

        return true;
	}
    return false;
}



void EditorVisuals::Save()
{
    ConsumePendingChanges();
    if (m_saveJob.Busy()) return;
    try {
        FileJobRequest request;
        request.output = m_loadedCharacter.filename;
        request.bytes = m_abstractEditor->BuildSaveSnapshot();
        request.codec = CompressionUtils::GetCompressionSetting(m_compressionIndex).compressionSetting;
        request.backup = request.backupOnlyIfNone = true;
        request.maxBackups = m_maxAutoSaves;
        m_jobRevision = m_editRevision; m_jobIsBackup = false;
        if (m_saveJob.Start(std::move(request))) { m_savingError = false; m_saveMessage.clear(); }
    } catch (const std::exception& error) { m_savingError = true; m_saveMessage = error.what(); }
}

void EditorVisuals::SaveBackup(bool onlyIfNone)
{
    ConsumePendingChanges();
    if (m_saveJob.Busy()) return;
    try {
        FileJobRequest request;
        request.output = m_loadedCharacter.filename;
        request.bytes = m_abstractEditor->BuildSaveSnapshot();
        request.backupOnly = true; request.backupOnlyIfNone = onlyIfNone;
        request.maxBackups = m_maxAutoSaves;
        m_jobRevision = m_editRevision; m_jobIsBackup = true;
        if (m_saveJob.Start(std::move(request))) { m_savingError = false; m_saveMessage.clear(); }
    } catch (const std::exception& error) {
        m_savingError = true; m_saveMessage = error.what();
        m_lastAutoSave = Helpers::getCurrentTimestamp();
    }
}

void EditorVisuals::MarkDirty()
{
    ++m_editRevision;
    m_lastChangeDate = Helpers::getCurrentTimestamp();
    m_savedLastChange = false;
    m_importNeeded = true;
}

void EditorVisuals::ConsumePendingChanges()
{
    for (auto form : m_structWindows) {
        if (!form->justAppliedChanges) continue;
        form->justAppliedChanges = false;
        MarkDirty();
    }
}

void EditorVisuals::PollSaveJob()
{
    if (auto result = m_saveJob.Poll()) {
        m_savingError = !result->ok && !result->cancelled;
        m_saveMessage = result->cancelled ? "Save cancelled." : result->error;
        m_lastAutoSave = Helpers::getCurrentTimestamp();
        if (result->ok) {
            m_backedUpRevision = m_jobRevision;
            if (!m_jobIsBackup) {
                m_savedLastChange = m_jobRevision == m_editRevision;
                m_loadedCharacter.lastSavedDate = Helpers::formatDateTime(result->date);
            }
        }
        if (m_closeAfterSave) {
            if (result->ok && m_savedLastChange) popen = false;
            m_closeAfterSave = false;
        }
    }
}

// -- Public methods -- //

EditorVisuals::~EditorVisuals()
{
	if (m_importerHelper) m_importerHelper->StopThreadAndCleanup();
	if (m_sharedMemHelper) m_sharedMemHelper->StopThreadAndCleanup();

    for (auto& win : m_structWindows) delete win;
    for (auto& win : m_subwindows) delete win;
    for (auto& [type, creator] : m_windowCreatorMap) delete creator.allocator;

}

EditorVisuals::EditorVisuals(const movesetInfo* movesetInfo, GameAddressesFile* addrFile, LocalStorage* storage)
{
	{
		// Init classes that interact with game process
		m_importerHelper = std::make_unique<GameImport>();
		m_sharedMemHelper = std::make_unique<GameSharedMem>();

		m_importerHelper->Init(addrFile, storage);
		m_sharedMemHelper->Init(addrFile, storage);

		m_importerHelper->StartThread();
		m_sharedMemHelper->synchronizeLockin = false;
		m_sharedMemHelper->StartThread();
	}

	{
		// Init editor logic class
		auto gameInfo = Games::GetGameInfoFromIdentifier(movesetInfo->gameId, movesetInfo->minorVersion);

		m_abstractEditor.reset(Games::FactoryGetEditorLogic(gameInfo, m_importerHelper->process, m_importerHelper->game));
		labels = std::make_unique<EditorLabel>(gameInfo, addrFile);
	}

	m_liveEditable = Games::IsGameLiveEditable(movesetInfo->gameId, movesetInfo->minorVersion);

	m_loadedCharacter.filename = movesetInfo->filename;
	m_loadedCharacter.wname = movesetInfo->wname;
	m_loadedCharacter.name = movesetInfo->name;
	m_loadedCharacter.lastSavedDate = Helpers::formatDateTime(movesetInfo->date);
	filename = m_loadedCharacter.filename.c_str();
	m_loadedCharacter.gameId = movesetInfo->gameId;

	m_subwindowsTitle = std::format("{}: {}", m_loadedCharacter.name.c_str(), _("edition.window_title"));
	m_windowTitle = std::format("{} {} - Moveset v{}", m_subwindowsTitle, PROGRAM_VERSION, movesetInfo->version_string);

	// Set the shared mem handler
	m_abstractEditor->SetSharedMemHandler(m_sharedMemHelper->GetSharedMemHandler());

	// Attempt to attach to game associated to moveset
	auto processList = GameProcessUtils::GetRunningProcessList();
	for (unsigned int gameIdx = 0; gameIdx < Games::GetGamesCount(); ++gameIdx)
	{
		auto gameInfo = Games::GetGameInfoFromIndex(gameIdx);

		const char* processName = gameInfo->processName;
		processEntry* p;

		// Detect if the game is running
		{
			bool isRunning = false;
			for (auto& process : processList)
			{
				if (process.name == processName)
				{
					p = &process;
					isRunning = true;
					break;
				}
			}
			if (!isRunning) {
				continue;
			}
		}

		if (!gameInfo->MatchesProcessWindowName(p->pid)) {
			continue;
		}

		if (gameInfo->SupportsGameImport(movesetInfo->gameId))
		{
			m_importerHelper->SetTargetProcess(gameInfo);
			if (gameInfo->onlineHandler != nullptr) {
				m_sharedMemHelper->SetTargetProcess(gameInfo);
			}
			else {
				m_sharedMemHelper->ResetTargetProcess();
			}
			DEBUG_LOG("Editor-compatible game '%s' already running: attaching.\n", processName);
			break;
		}
	}

	m_lastAutoSave = Helpers::getCurrentTimestamp();
	m_lastChangeDate = m_lastAutoSave;
}

void EditorVisuals::IssueFieldUpdate(EditorWindowType winType, int valueChange, int listStart, int listEnd)
{
	DEBUG_LOG("EditorVisuals::IssueFieldUpdate(type: %u, change: %d, list start: %d, list end: %d)\n", winType, valueChange, listStart, listEnd);
	for (auto& window : m_structWindows)
	{
		if (window->popen) {
			window->RequestFieldUpdate(winType, valueChange, listStart, listEnd);
		}
	}
}

void EditorVisuals::RenderSubwindows()
{
	bool window_focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

	for (size_t i = 0; i < m_structWindows.size();)
	{
		EditorForm* moveWin = m_structWindows[i];


		if (moveWin->popen)
		{
			moveWin->Render(m_windowFocused);
			if (moveWin->windowFocused) {
				window_focused = true;
			}

			if (moveWin->moveFocusAway) {
				moveWin->moveFocusAway = false;
				ImGui::SetKeyboardFocusHere();
			}

			++i;
		}
		else {
			m_structWindows.erase(m_structWindows.begin() + i);
			delete moveWin;
		}
	}

	for (size_t i = 0; i < m_subwindows.size();)
	{
		EditorSubwindow* window = m_subwindows[i];

		if (window->Render()) {
			++i;
		}
		else {
			m_subwindows.erase(m_subwindows.begin() + i);
			delete window;
		}
	}

	m_windowFocused = window_focused;
}

void EditorVisuals::DetectKeyboardShortcuts()
{
	const std::string* identifier = Keybinds::DetectKeybindPress();
	if (identifier != nullptr) {
		OnKeyboardShortcut(*identifier);
	}
}

void EditorVisuals::Render(int dockid)
{
    ConsumePendingChanges();
    PollSaveJob();
    if (m_importerHelper && m_sharedMemHelper) {
        auto state0 = m_importerHelper->LockState();
        auto state1 = m_sharedMemHelper->LockState();
        RenderGameSpecific(dockid);
    } else RenderGameSpecific(dockid);
}


void EditorVisuals::AddSubwindow(EditorSubwindow* new_win)
{
    std::unique_ptr<EditorSubwindow> window(new_win);
    if (!window) throw std::invalid_argument("Cannot publish a null editor subwindow.");
    window->nextDockId = m_dockId;
    m_subwindows.push_back(window.get());
    window.release();
}

bool EditorVisuals::RefreshSubwindowIfExists(const std::string& identifier)
{
	for (const auto& win : m_subwindows)
	{
		if (win->identifier == identifier) {
			win->Refresh(m_dockId);
			return true;
		}
	}
	return false;
}
namespace {
struct FormTransaction final : EditorForm::TransactionState {
    std::vector<std::unique_ptr<EditorForm::TransactionState>> states;
    void Commit() noexcept override { for (auto& state : states) state->Commit(); }
};
}
std::unique_ptr<EditorForm::TransactionState> EditorVisuals::BeginFormTransaction(std::span<EditorForm* const> forms) {
    auto result = std::make_unique<FormTransaction>();
    result->states.reserve(forms.size());
    size_t budget = 64 * 1024 * 1024;
    std::set<EditorForm*> seen;
    for (auto* form : forms) {
        if (!form) throw std::invalid_argument("Cannot checkpoint a null editor form.");
        if (seen.insert(form).second) result->states.push_back(form->BeginTransaction(budget));
    }
    return result;
}
struct EditorVisuals::WindowState final : EditorForm::TransactionState {
    EditorVisuals& owner;
    std::vector<EditorForm*> forms;
    std::vector<EditorSubwindow*> subwindows;
    std::set<EditorForm*> originalForms;
    std::set<EditorSubwindow*> originalSubwindows;
    bool committed = false;
    explicit WindowState(EditorVisuals& target) : owner(target), forms(target.m_structWindows), subwindows(target.m_subwindows),
        originalForms(forms.begin(), forms.end()), originalSubwindows(subwindows.begin(), subwindows.end()) {}
    ~WindowState() override {
        if (committed) return;
        for (auto* form : owner.m_structWindows) if (!originalForms.contains(form)) delete form;
        for (auto* window : owner.m_subwindows) if (!originalSubwindows.contains(window)) delete window;
        forms.swap(owner.m_structWindows); subwindows.swap(owner.m_subwindows);
    }
    void Commit() noexcept override { committed = true; }
};
std::unique_ptr<EditorForm::TransactionState> EditorVisuals::BeginWindowTransaction() {
    return std::make_unique<WindowState>(*this);
}
std::unique_ptr<EditorForm::TransactionState> EditorVisuals::BeginFormTransaction(EditorForm& source) {
    auto result = std::make_unique<FormTransaction>();
    result->states.reserve(2);
    result->states.push_back(BeginWindowTransaction());
    std::vector<EditorForm*> forms = m_structWindows;
    forms.push_back(&source);
    result->states.push_back(BeginFormTransaction(forms));
    return result;
}

void EditorVisuals::PublishFormWindow(std::unique_ptr<EditorForm> form) {
    m_structWindows.push_back(form.get());
    form.release();
}
