#include <ImGui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "MainWindow.hpp"
#include "GameData.hpp"
#include "GameExtract.hpp"
#include "GameProcess.hpp"

MainWindow::MainWindow(GLFWwindow* window, const char* c_glsl_version)
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();

	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init(c_glsl_version);

	ImGui::StyleColorsDark();

	// Setup ImGui config
	io.IniFilename = nullptr; //I don't want to save settings (for now). Perhaps save in appdata later.
	//io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	// Init subclasses of singletons, start threads too
	{
        // todo: delete
		extractor = &GameExtract::getInstance();
		GameProcess* process = new GameProcess();
		extractor->process = process;
		extractor->game = new GameData(process);
		extractor->ReloadMovesetList();
		extractor->StartThread();
	}
}

void MainWindow::NewFrame()
{
	// I believe this inits the current frame buffer
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
}

void MainWindow::Update(int width, int height)
{
	// Actual rendering stuff
	const float c_statusBarHeight = 30;

	float navMenuWidth;

	// Navbar
	{
		ImGui::SetNextWindowPos(ImVec2(0, 0));
		ImGui::SetNextWindowSizeConstraints(ImVec2(-1, height - c_statusBarHeight), ImVec2(-1, height - c_statusBarHeight));
		ImGui::Begin("Navbar", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav);
		navMenu.Render();
		navMenuWidth = ImGui::GetWindowWidth();
		ImGui::End();
	}

	// Menus
	{
		ImGui::SetNextWindowPos(ImVec2(navMenuWidth, 0));
		ImGui::SetNextWindowSizeConstraints(ImVec2(width - navMenuWidth, height - c_statusBarHeight), ImVec2(width - navMenuWidth, height - c_statusBarHeight));
		ImGui::Begin("Tools", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav);
		switch (navMenu.menuId)
		{
		case NAV__MENU_EXTRACT:
			extractMenu.Render();
			break;
		case NAV__MENU_IMPORT:
			importMenu.Render();
			break;
		case NAV__MENU_ONLINE_PLAY:
			onlineMenu.Render();
			break;
		case NAV__MENU_CAMERA:
			break;
		case NAV__MENU_EDITION:
			break;
		case NAV__MENU_DOCUMENTATION:
			break;
		case NAV__MENU_ABOUT:
			break;
		}
		ImGui::End();
	}

	// Status bar
	{
		ImGui::SetNextWindowPos(ImVec2(0.0f, height - c_statusBarHeight));
		ImGui::SetNextWindowSizeConstraints(ImVec2((float)width, c_statusBarHeight), ImVec2((float)width, c_statusBarHeight));
		ImGui::Begin("StatusBar", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav);
		statusBar.Render();
		ImGui::End();
	}
}

void MainWindow::Render()
{
	// Render the frame buffer
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void MainWindow::Shutdown()
{
	// Cleanup of everything we do should be done here

	extractor->StopThread();
	delete extractor->process;
	delete extractor->game;

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
}