// CMakeProject1.cpp : définit le point d'entrée de l'application.

#include "glad/glad.h"
#include "GLFW/glfw3.h"

#include <stdio.h>
#include <format>
#include <windows.h>
#include <filesystem>

#include "MainWindow.hpp"
#include "Localization.hpp"
#include "GameProcess.hpp"
#include "GameAddressesFile.hpp"

#include "constants.h"

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

	// Initialize GLFW library
	glfwSetErrorCallback(glfw_error_callback);
	if (!glfwInit()) {
		return 1;
	}

	// GL 3.0 + GLSL 130
	// Set the window hint. Not really that important to be honest.
	const char* c_glsl_version = "#version 130";
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
	//glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
	//glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only

	// Setup window title and create window
	std::string windowTitle = std::format("{} {}", PROGRAM_TITLE, PROGRAM_VERSION);
	GLFWwindow* window = glfwCreateWindow(PROGRAM_WIN_WIDTH, PROGRAM_WIN_HEIGHT, windowTitle.c_str(), nullptr, nullptr);

	if (window == nullptr) {
		return 2;
	}

	// Set window for current thread
	glfwMakeContextCurrent(window);
	// Enable vsync
	glfwSwapInterval(1);

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
		throw("Unable to context to OpenGL");
	}

	{
		// Set viewport for OpenGL
		int screen_width, screen_height;
		glfwGetFramebufferSize(window, &screen_width, &screen_height);
		glViewport(0, 0, screen_width, screen_height);
	}

	// Load translation
	Localization::LoadFile(PROGRAM_DEFAULT_LANG);
	// Load game addresses
	GameAddressesFile::LoadFile();

	// Init main program. This will get most things going and create the important threads
	MainWindow program(window, c_glsl_version);

	while (!glfwWindowShouldClose(window)) {
		// Poll and handle events such as MKB inputs, window resize. Required
		glfwPollEvents();
		
		// Set window BG
		glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
		glClear(GL_COLOR_BUFFER_BIT);

		int width, height;
		glfwGetWindowSize(window, &width, &height);

		// Probably framebuffer related? Required
		program.NewFrame();
		
		// Main layout function : every we display is in there
		program.Update(width, height);

		// Finishing touch. There shouldn't be any reason to ever touch these.
		program.Render();
		glfwSwapBuffers(window);
	}

	// Cleanup what needs to be cleaned up
	program.Shutdown();

	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}
