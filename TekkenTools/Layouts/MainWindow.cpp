#include <ImGui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "MainWindow.h"

MainWindow::MainWindow(GLFWwindow* window, const char* GLSL_VERSION)
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();

	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init(GLSL_VERSION);

	ImGui::StyleColorsDark();

	io.IniFilename = nullptr; //I don't want to save settings (for now). Perhaps save in appdata later.
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
}

void MainWindow::NewFrame()
{
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
}

void MainWindow::Update(const int WIDTH, const int HEIGHT)
{
	const float c_statusBarHeight = 30;

	float navMenuWidth;

	// Navbar
	{
		ImGui::SetNextWindowPos(ImVec2(0, 0));
		ImGui::SetNextWindowSizeConstraints(ImVec2(-1, HEIGHT - c_statusBarHeight), ImVec2(-1, WIDTH - c_statusBarHeight));
		ImGui::Begin("Navbar", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav);
		navMenu.Render();
		navMenuWidth = ImGui::GetWindowWidth();
		ImGui::End();
	}

	// Menus
	{
		ImGui::SetNextWindowPos(ImVec2(navMenuWidth, 0));
		ImGui::SetNextWindowSizeConstraints(ImVec2(WIDTH - navMenuWidth, HEIGHT - c_statusBarHeight), ImVec2(WIDTH - navMenuWidth, HEIGHT - c_statusBarHeight));
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
		ImGui::SetNextWindowPos(ImVec2(0, HEIGHT - c_statusBarHeight));
		ImGui::SetNextWindowSizeConstraints(ImVec2(WIDTH, c_statusBarHeight), ImVec2(WIDTH, c_statusBarHeight));
		ImGui::Begin("StatusBar", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav);
		statusBar.Render();
		ImGui::End();
	}
}

void MainWindow::Render()
{
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void MainWindow::Shutdown()
{
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
}