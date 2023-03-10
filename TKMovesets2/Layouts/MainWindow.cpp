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
		// Maybe keep the status bar for specific events such as compression
		const int c_statusBarHeight = 0; // 30
		const int navMenuWidth = 160;

		ImGuiViewport* mainView = ImGui::GetMainViewport();
		const float width = mainView->Size.x;
		const float height = mainView->Size.y;

		// Navbar
		if (navMenuWidth > 0)
		{
			ImGui::SetNextWindowPos(mainView->Pos);
			ImGui::SetNextWindowSize(ImVec2(navMenuWidth, height - c_statusBarHeight));
			ImGui::Begin("Navbar", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoDocking);
			navMenu.Render(navMenuWidth - 10);
			ImGui::End();
		}


		// Menus
		{
			ImGui::SetNextWindowPos(ImVec2(mainView->Pos.x + navMenuWidth, mainView->Pos.y));
			ImGui::SetNextWindowSize(ImVec2(width - navMenuWidth, height - c_statusBarHeight));
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
				editionMenu.Render();
				break;
			case NAV__MENU_DOCUMENTATION:
				break;
			case NAV__MENU_ABOUT:
				RenderSubmenu_About();
				break;
			}

			ImGui::End();
		}

		// Status bar, currently not useful
		if (c_statusBarHeight > 0)
		{
			ImGui::SetNextWindowPos(ImVec2(0.0f, height - c_statusBarHeight));
			ImGui::SetNextWindowSizeConstraints(ImVec2((float)width, c_statusBarHeight), ImVec2((float)width, c_statusBarHeight));
			ImGui::Begin("StatusBar", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoDocking);
			//statusBar.Render();
			ImGui::End();
		}

	}

	/*
	ImGui::Begin("MainWindow22", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoDocking);
	ImGui::Text("yfsdefdsfdsfdso");
	ImGui::Button("gkioregoregore");
	ImGui::End();
	*/


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