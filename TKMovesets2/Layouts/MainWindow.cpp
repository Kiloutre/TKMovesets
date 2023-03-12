#pragma warning( disable: 4244 )

#include <ImGui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "MainWindow.hpp"
#include "GameData.hpp"
#include "GameExtract.hpp"
#include "GameProcess.hpp"
// Submenus
#include "Submenu_About.hpp"
#include "imgui_extras.hpp"

const ImU32 editorTitleColors[] = {
	IM_COL32(77, 131, 219, 125),
	IM_COL32(119, 58, 199, 125),
	IM_COL32(210, 100, 222, 125),
	IM_COL32(186, 54, 54, 125),
	IM_COL32(140, 0, 0, 125),
	IM_COL32(119, 175, 58, 125),
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

	// todo: attempt to load. popup if fail. open window and std vector push back if not
	try {
		EditorWindow* newWin = new EditorWindow(movesetInfos, addrFile, &storage);
		editorWindows.push_back(newWin);
	}
	catch(EditorWindow_MovesetLoadFail) {
		// Memory was freed so no need to do anything
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

	// Font import example
	//io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/micross.ttf", 20, NULL, io.Fonts->GetGlyphRangesDefault());
	//io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/micross.ttf", 20, NULL, io.Fonts->GetGlyphRangesJapanese());
	//io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/micross.ttf", 20, NULL, io.Fonts->GetGlyphRangesKorean());
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
			navMenu.Render(navMenuWidth - 10);
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
				const ImU32 colorCount = sizeof(editorTitleColors) / sizeof(editorTitleColors[0]);
				ImGui::PushStyleColor(ImGuiCol_TitleBg, editorTitleColors[i % colorCount]);
				ImGui::PushStyleColor(ImGuiCol_TitleBgActive, editorTitleColors[i % colorCount]);
				ImGui::PushStyleColor(ImGuiCol_Tab, editorTitleColors[i % colorCount]);
				ImGui::PushStyleColor(ImGuiCol_TabActive, editorTitleColors[i % colorCount]);
				w->Render(i + 2);
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

	// Do this here so that we're not modifying the moveset list at the same time as we're displaying it
	storage.CleanupUnusedMovesetInfos();
}

void MainWindow::Shutdown()
{
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
}