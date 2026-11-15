#include <ImGui.h>
#include "Localization.hpp"
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "MainWindow.hpp"
#include "GameData.hpp"
#include "GameExtract.hpp"
#include "GameProcess.hpp"
// Submenus
#include "Submenu_About.hpp"
// Other
#include "Helpers.hpp"
#include "imgui_extras.hpp"
#include "OpenRequests.hpp"
#include <filesystem>
#include <system_error>

// todo: fix inactive/collapsed tabs looking too similar to active ones
const ImU32 editorTitleColors[] = {
	IM_COL32(77, 131, 219, 125),
	IM_COL32(119, 58, 199, 125),
	IM_COL32(210, 100, 222, 125),
	IM_COL32(186, 54, 54, 125),
	IM_COL32(140, 0, 0, 125),
	IM_COL32(119, 175, 58, 125),
};
const ImU32 editorTitleInactiveColors[] = {
	IM_COL32(36, 64, 107, 0),
	IM_COL32(119, 58, 199, 0),
	IM_COL32(210, 100, 222, 0),
	IM_COL32(186, 54, 54, 0),
	IM_COL32(140, 0, 0, 0),
	IM_COL32(119, 175, 58, 0),
};

// -- Private methods -- //

//
void MainWindow::LoadMovesetEditor(const movesetInfo* movesetInfos)
{
	for (const auto& win : m_editorWindows)
	{
		if (win->filename == movesetInfos->filename)
		{
			win->setFocus = true;
			// Moveset already opened in editor, don't open it again
			return;
		}
	}

	try {
		auto game = Games::GetGameInfoFromIdentifier(movesetInfos->gameId, movesetInfos->minorVersion);
		auto newWin = std::unique_ptr<EditorVisuals>(Games::FactoryGetEditorVisuals(game, movesetInfos, m_addrFile.get(), &m_storage));
		m_editorWindows.push_back(std::move(newWin));
	}
	catch(EditorWindow_MovesetLoadFail) {
        DEBUG_ERR("Failed to load editor for moveset '%S'", movesetInfos->filename.c_str());
		// A throw in a constructor frees its memory so no need to do anything
	}
}

bool MainWindow::IsMovesetOpenInEditor(const std::wstring& filename) const
{
	for (const auto& win : m_editorWindows)
	{
		if (!win || !win->filename) continue;
		if (_wcsicmp(win->filename, filename.c_str()) == 0) return true;
		std::error_code error;
		if (std::filesystem::equivalent(std::filesystem::path(win->filename), std::filesystem::path(filename), error) && !error) return true;
	}
	return false;
}

void MainWindow::HandleOpenRequests()
{
	const auto now = std::chrono::steady_clock::now();
	for (auto& path : OpenRequests::Take()) {
		m_pendingOpens.push_back({ std::move(path), now + std::chrono::seconds(10) });
	}
	if (m_pendingOpens.empty()) return;

	// The moveset list fills in from its own thread, so a request may wait a few frames for its file to be listed
	std::erase_if(m_pendingOpens, [&](const PendingOpen& request) {
		for (movesetInfo* moveset : m_storage.extractedMovesets) {
			if (_wcsicmp(moveset->filename.c_str(), request.path.c_str()) != 0) continue;
			SetActiveMenu(NAV__MENU_EDITION);
			// Same rule as the edition menu's Edit button
			if (moveset->editable) LoadMovesetEditor(moveset);
			return true;
		}
		if (now < request.deadline) return false;
		DEBUG_ERR("Moveset '%S' never appeared in the moveset list, not opening it", request.path.c_str());
		return true;
	});
}

static void TryLoadWindowsFont(std::vector<const char*> filenames, float size_pixels, ImGuiIO& io, ImFontConfig* config, const ImWchar* glyphRange)
{
	for (auto& name : filenames)
	{
		std::string full_filename = "C:/Windows/Fonts/" + std::string(name);
		if (!Helpers::fileExists(full_filename.c_str())) {
			continue;
		}

		DEBUG_LOG("Loaded font %s\n", full_filename.c_str());
		io.Fonts->AddFontFromFileTTF(full_filename.c_str(), size_pixels, config, glyphRange);
	}
}

// -- Public methods -- //

MainWindow::MainWindow(bool offline)
{
	// ImGUI config init
	ImGuiIO& io = ImGui::GetIO();

	ImFont* font = io.Fonts->AddFontDefault();

	ImFontConfig config;
	config.MergeMode = true;

	TryLoadWindowsFont({ "msgothic.ttc" }, 15, io, &config, io.Fonts->GetGlyphRangesJapanese());
	TryLoadWindowsFont({ "CascadiaMono.ttf" }, 15, io, &config, io.Fonts->GetGlyphRangesCyrillic());
	TryLoadWindowsFont({ "malgun.ttf" }, 16, io, &config, io.Fonts->GetGlyphRangesKorean());


	ImGuiExtra::InitMarkdownConfig();
	if (!(io.BackendFlags & ImGuiBackendFlags_RendererHasTextures)) io.Fonts->Build();


	// Program config (various threads, submenus)
	m_storage.ReloadMovesetList();
	m_storage.CleanupUnusedMovesetInfos();
	// Start in the editor when there is a moveset to edit, else in the extractor to get one.
	SetActiveMenu(m_storage.extractedMovesets.empty() ? NAV__MENU_EXTRACT : NAV__MENU_EDITION);

	m_addrFile = std::make_unique<GameAddressesFile>(!offline);

	m_extractor.Init(m_addrFile.get(), &m_storage);
	m_importer.Init(m_addrFile.get(), &m_storage);
	m_sharedMem.Init(m_addrFile.get(), &m_storage);

	m_onlineMenu.gameHelper = &m_sharedMem;
	m_persistentPlayMenu.gameHelper = &m_sharedMem;
	m_editionMenu.isOpenInEditor = [this](const std::wstring& filename) { return IsMovesetOpenInEditor(filename); };

	sideMenu.requestedUpdatePtr = &requestedUpdate;
	sideMenu.storage = &m_storage;
	if (!offline) sideMenu.SetAddrFile(m_addrFile.get());

	{
		if (offline) return;
		// Detect running games and latch on to them if possible
		bool attachedExtractor = false;
		bool attachedImporter = false;
		bool attachedOnline = false;

		auto processList = GameProcessUtils::GetRunningProcessList();

		// Loop through every game we support
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

			if (!attachedExtractor && gameInfo->extractor != nullptr) {
				m_extractor.SetTargetProcess(gameInfo);
				attachedExtractor = true;
				DEBUG_LOG("Extraction-compatible game '%s' already running: attaching.\n", processName);
			}

			if (!attachedImporter && gameInfo->importer != nullptr) {
				m_importer.SetTargetProcess(gameInfo);
				attachedImporter = true;
				DEBUG_LOG("Importation-compatible game '%s' already running: attaching.\n", processName);
			}

			if (!attachedOnline && gameInfo->onlineHandler != nullptr) {
				m_sharedMem.SetTargetProcess(gameInfo);
				attachedOnline = true;
				DEBUG_LOG("Online-compatible game '%s' already running: attaching.\n", processName);
			}

		}
	}

	m_storage.StartThread();
	m_extractor.StartThread();
	m_importer.StartThread();
	m_sharedMem.StartThread();
}

// Actual rendering function
void MainWindow::Update()
{
	HandleOpenRequests();

	auto state0 = m_extractor.LockState();
	auto state1 = m_importer.LockState();
	auto state2 = m_sharedMem.LockState();
	// Main window rendering
	ImGuiViewport* mainView = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(mainView->Pos);
	ImGui::SetNextWindowSize(mainView->Size);


	if (ImGui::Begin("###MainWindow", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoDocking))
	{
		const int sidebarWidth = 160;

		const float width = mainView->Size.x;
		const float height = mainView->Size.y;

		// Sidebar
		{
			float sideMenuWidth = (float)(sidebarWidth - 10);

			ImGui::BeginChild("SideBar", ImVec2(sidebarWidth, 0), true, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoDocking);

			// Render nav menu
			m_navMenu.Render(sideMenuWidth, m_persistentPlayMenu.gameHelper->lockedIn);

			ImGui::NewLine();
			ImGui::NewLine();
			ImGui::Separator();

			// Render side menu
			sideMenu.Render(sideMenuWidth);

			ImGui::EndChild();
		}


		// Menus
		{
			ImGui::SameLine();
			ImGui::BeginChild("Tools", ImVec2(width - sidebarWidth - 25, 0), false, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoDocking);

			switch (m_navMenu.menuId)
			{
			case NAV__MENU_EXTRACT:
				m_extractMenu.Render(m_extractor);
				break;
			case NAV__MENU_IMPORT:
				m_importMenu.Render(m_importer);
				break;
			case NAV__MENU_ONLINE_PLAY:
				m_onlineMenu.Render();
				break;
			case NAV__MENU_PERSISTENT_PLAY:
				m_persistentPlayMenu.Render();
				break;
			case NAV__MENU_CAMERA:
				break;
			case NAV__MENU_EDITION:
				{
					movesetInfo* moveset = m_editionMenu.Render(m_storage);
					if (moveset != nullptr) {
						LoadMovesetEditor(moveset);
					}
				}
				break;
			case NAV__MENU_DOCUMENTATION:
				break;
			case NAV__MENU_ADDRESSES:
				if (m_addrFile) m_addressesMenu.Render(*m_addrFile, m_extractor.IsBusy() || m_importer.IsBusy() || m_sharedMem.IsBusy());
				break;
			case NAV__MENU_EXTRA:
				if (m_addrFile) m_extraMenu.Render(*m_addrFile);
				break;
			case NAV__MENU_ABOUT:
				RenderSubmenu_About();
				break;
			}

			ImGui::EndChild();
		}

		// Editor windows
		for (unsigned int i = 0; i < m_editorWindows.size();)
		{
			EditorVisuals* w = m_editorWindows[i].get();

			if (w->popen) {
				const ImU32 colorCount = _countof(editorTitleColors);

				auto inactiveTitleCol = editorTitleInactiveColors[i % colorCount];
				auto titleCol = editorTitleColors[i % colorCount];

				ImGui::PushStyleColor(ImGuiCol_TitleBgCollapsed, inactiveTitleCol);
				ImGui::PushStyleColor(ImGuiCol_TitleBg, inactiveTitleCol);
				ImGui::PushStyleColor(ImGuiCol_TitleBgActive, titleCol);
				ImGui::PushStyleColor(ImGuiCol_TabUnfocused, inactiveTitleCol);
				ImGui::PushStyleColor(ImGuiCol_Tab, inactiveTitleCol);
				ImGui::PushStyleColor(ImGuiCol_TabActive, titleCol);
				w->Render(i + 2);
				ImGui::PopStyleColor();
				ImGui::PopStyleColor();
				ImGui::PopStyleColor();
				ImGui::PopStyleColor();
				ImGui::PopStyleColor();
				ImGui::PopStyleColor();
				++i;
			}
			else {
				// Window was closed, close the associated editor and free its ressources
				m_editorWindows.erase(m_editorWindows.begin() + i);
			}
		}

		ImGui::End();
	}

	// -- Rendering end -- //
	{
		// Cleanup stuff that we really don't want to clean up during Render()
		m_storage.CleanupUnusedMovesetInfos();
		m_extractor.FreeExpiredFactoryClasses();
		m_importer.FreeExpiredFactoryClasses();
		m_sharedMem.FreeExpiredFactoryClasses();
	}
}

void MainWindow::NewFrame()
{
	// I believe this inits the current frame buffer
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
}

MainWindow::~MainWindow()
{
	sideMenu.CleanupThread();

	m_extractor.StopThreadAndCleanup();
	m_importer.StopThreadAndCleanup();
	m_sharedMem.StopThreadAndCleanup();

	m_editorWindows.clear();

	// Once every thread that may use storage has been stopped, we can finally stop storage
	m_storage.StopThreadAndCleanup();
}
bool MainWindow::ConfirmExit(bool requested) {
    if (requested || requestedUpdate) { m_exitPending=true; m_exitUpdate|=requestedUpdate; requestedUpdate=false; }
    if (!m_exitPending) return false;
    bool dirty=false,busy=false;
    for (const auto& editor:m_editorWindows) { dirty|=editor->HasPendingEdits(); busy|=editor->SaveBusy(); }
    if (!dirty && !busy) { requestedUpdate=m_exitUpdate; return true; }
    ImGui::OpenPopup("###CloseApplication");
    bool approved=false;
    if (ImGuiExtra::BeginFittedModal("CloseApplication",_("close.title"),nullptr,30.0f)) {
        ImGui::TextUnformatted(busy?_("close.busy"):_("close.unsaved"));
        ImGui::BeginDisabled(busy);
        if (ImGui::Button(_("close.discard"))) { requestedUpdate=m_exitUpdate; approved=true; ImGui::CloseCurrentPopup(); }
        ImGui::EndDisabled(); ImGui::SameLine();
        if (ImGui::Button(_("close.keep_editing"))) { m_exitPending=m_exitUpdate=false; ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }
    return approved;
}
