#include <ImGui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "MainWindow.h"

MainWindow::MainWindow(GLFWwindow* window, const char* glsl_version)
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();

	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init(glsl_version);

	ImGui::StyleColorsDark();

	io.IniFilename = NULL; //I don't want to save settings (for now). Perhaps save in appdata later.
}

void MainWindow::NewFrame()
{
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
}

void MainWindow::Update()
{
	// Interface should be done here
	ImGui::SetNextWindowSize(ImVec2(1280, 720));
	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::Begin("MainWindow", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav);

	{
		ImGui::Columns(2);
		ImGui::SetColumnOffset(1, 230);
		// Render navigation menu
		navMenu.Render();

		// Render submenus
		ImGui::NextColumn();
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
	}

	ImGui::End(); 
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