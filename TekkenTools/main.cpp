// CMakeProject1.cpp : définit le point d'entrée de l'application.

#include "glad/glad.h"
#include "GLFW/glfw3.h"

#include <stdio.h>
#include <format>
#include <windows.h>
#include <filesystem>

#include "constants.h"
#include "MainWindow.hpp"
#include "Localization.hpp"
#include "GameProcess.hpp"
#include "GameAddressesFile.hpp"

using namespace std;

static void glfw_error_callback(int error, const char* description)
{
	fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

int main(int argc, wchar_t** argv)
{
	{
		// Make sure working dir is same as .exe
		wchar_t currPath[MAX_PATH] = { 0 };
		GetModuleFileNameW(nullptr, currPath, MAX_PATH);
		wstring ws(currPath);
		string newPath(ws.begin(), ws.end());
		newPath.erase(newPath.find_last_of("\\"));
		std::filesystem::current_path(newPath);
	}

	glfwSetErrorCallback(glfw_error_callback);
	// Setup window
	if (!glfwInit()) {
		return 1;
	}

	// GL 3.0 + GLSL 130
	const char* c_glsl_version = "#version 130";
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
	//glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
	//glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only

	// Create window
	std::string windowTitle = std::format("{} {}", PROGRAM_TITLE, PROGRAM_VERSION);
	GLFWwindow* window = glfwCreateWindow(PROGRAM_WIN_WIDTH, PROGRAM_WIN_HEIGHT, windowTitle.c_str(), nullptr, nullptr);

	if (window == nullptr) {
		return 2;
	}

	glfwMakeContextCurrent(window);
	// Enable vsync
	glfwSwapInterval(1);

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
		throw("Unable to context to OpenGL");
	}

	{
		int screen_width, screen_height;
		glfwGetFramebufferSize(window, &screen_width, &screen_height);
		glViewport(0, 0, screen_width, screen_height);
	}

	// Load translation
	Localization::LoadFile(PROGRAM_DEFAULT_LANG);
	// Load game addresses
	GameAddressesFile::LoadFile();
	// Start the thread that will seek the game progress and attach to it
	GameProcess::getInstance().StartAttachingThread();

	MainWindow program(window, c_glsl_version);

	while (!glfwWindowShouldClose(window)) {
		// Poll and handle events such as MKB inputs, window resize
		glfwPollEvents();
		
		// Set window BG
		glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
		glClear(GL_COLOR_BUFFER_BIT);

		int width, height;
		glfwGetWindowSize(window, &width, &height);

		program.NewFrame();
		
		// Main layout function : every we display is in there
		program.Update(width, height);

		program.Render();
		glfwSwapBuffers(window);
	}

	program.Shutdown();

	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}
