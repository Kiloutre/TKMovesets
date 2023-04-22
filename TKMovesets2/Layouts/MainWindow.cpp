#include <ImGui.h>
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
void MainWindow::LoadMovesetEditor(movesetInfo* movesetInfos)
{
	for (EditorWindow* win : editorWindows)
	{
		if (win->filename == movesetInfos->filename)
		{
			win->setFocus = true;
			// Moveset already opened in editor, don't open it again
			return;
		}
	}

	try {
		EditorWindow* newWin = new EditorWindow(movesetInfos, addrFile, &storage);
		editorWindows.push_back(newWin);
	}
	catch(EditorWindow_MovesetLoadFail) {
		// Memory was freed so no need to do anything
	}
}

static void TryLoadWindowsFont(std::vector<const char*> filenames, ImGuiIO& io, ImFontConfig* config, const ImWchar* glyphRange)
{
	for (auto& name : filenames)
	{
		std::string full_filename = "C:/Windows/Fonts/" + std::string(name);
		if (!Helpers::fileExists(full_filename.c_str())) {
			continue;
		}

		DEBUG_LOG("Loaded font %s\n", full_filename.c_str());
		io.Fonts->AddFontFromFileTTF(full_filename.c_str(), 15, config, glyphRange);
	}
}

// -- Public methods -- //

MainWindow::MainWindow(GLFWwindow* window, const char* c_glsl_version)
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();

	// Setup ImGui config. This had to be done before initializing the backends
	ImGuiIO& io = ImGui::GetIO();
	io.IniFilename = nullptr; //I don't want to save settings (for now). Perhaps save in appdata later.
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	// Initialize backends
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init(c_glsl_version);

	// Font import
	ImFont* font = io.Fonts->AddFontDefault();
	ImFontConfig config;
	config.MergeMode = true;

	// Attempt to load fonts for specific glyph ranges, loading the first foond that is found in the provided lists
	TryLoadWindowsFont({ "msgothic.ttc" }, io, &config, io.Fonts->GetGlyphRangesJapanese());
	TryLoadWindowsFont({ "CascadiaMono.ttf" }, io, &config, io.Fonts->GetGlyphRangesCyrillic());
	TryLoadWindowsFont({ "malgun.ttf" }, io, &config, io.Fonts->GetGlyphRangesKorean());

	io.Fonts->Build();
}

void MainWindow::NewFrame()
{
	// I believe this inits the current frame buffer
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
}

// Actual rendering function
void MainWindow::Update()
{
	// Main window rendering
	{
		const int navMenuWidth = 160;

		ImGuiViewport* mainView = ImGui::GetMainViewport();
		const float width = mainView->Size.x;
		const float height = mainView->Size.y;

		// Navbar
		if (navMenuWidth > 0)
		{
			ImGui::SetNextWindowPos(mainView->Pos);
			ImGui::SetNextWindowSize(ImVec2(navMenuWidth, height));
			ImGui::Begin("Navbar", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoDocking);
			navMenu.Render((float)(navMenuWidth - 10), persistentPlayMenu.gameHelper->lockedIn);
			ImGui::End();
		}


		// Menus
		{
			ImGui::SetNextWindowPos(ImVec2(mainView->Pos.x + navMenuWidth, mainView->Pos.y));
			ImGui::SetNextWindowSize(ImVec2(width - navMenuWidth, height));
			ImGui::Begin("Tools", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoDocking);

			switch (navMenu.menuId)
			{
			case NAV__MENU_EXTRACT:
				extractMenu.Render(extractor);
				break;
			case NAV__MENU_IMPORT:
				importMenu.Render(importer);
				break;
			case NAV__MENU_ONLINE_PLAY:
				onlineMenu.Render();
				break;
			case NAV__MENU_PERSISTENT_PLAY:
				persistentPlayMenu.Render();
				break;
			case NAV__MENU_CAMERA:
				break;
			case NAV__MENU_EDITION:
				{
					movesetInfo* moveset = editionMenu.Render(storage);
					if (moveset != nullptr) {
						LoadMovesetEditor(moveset);
					}
				}
				break;
			case NAV__MENU_DOCUMENTATION:
				break;
			case NAV__MENU_ABOUT:
				RenderSubmenu_About();
				break;
			}

			ImGui::End();
		}

		// Editor windows
		for (unsigned int i = 0; i < editorWindows.size();)
		{
			EditorWindow* w = editorWindows[i];

			if (w->popen) {
				const ImU32 colorCount = _countof(editorTitleColors);
				ImGui::PushStyleColor(ImGuiCol_TitleBgCollapsed, editorTitleInactiveColors[i % colorCount]);
				ImGui::PushStyleColor(ImGuiCol_TitleBg, editorTitleInactiveColors[i % colorCount]);
				ImGui::PushStyleColor(ImGuiCol_TitleBgActive, editorTitleColors[i % colorCount]);
				ImGui::PushStyleColor(ImGuiCol_TabUnfocused, editorTitleInactiveColors[i % colorCount]);
				ImGui::PushStyleColor(ImGuiCol_Tab, editorTitleInactiveColors[i % colorCount]);
				ImGui::PushStyleColor(ImGuiCol_TabActive, editorTitleColors[i % colorCount]);
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
				editorWindows.erase(editorWindows.begin() + i);
				delete w;
			}
		}
	}

	// -- Rendering end -- //
	{
		// Cleanup stuff that we really don't want to clean up during Render()
		storage.CleanupUnusedMovesetInfos();
		extractor.FreeExpiredFactoryClasses();
		importer.FreeExpiredFactoryClasses();
		sharedMem.FreeExpiredFactoryClasses();
	}
}

void MainWindow::Shutdown()
{
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
}