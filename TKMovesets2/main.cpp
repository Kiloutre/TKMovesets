// CMakeProject1.cpp : définit le point d'entrée de l'application.

#include "glad/glad.h"
#include "GLFW/glfw3.h"

#include <stdio.h>
#include <format>
#include <windows.h>
#include <locale>
#include <filesystem>

#include "MainWindow.hpp"
#include "Localization.hpp"
#include "GameAddressesFile.hpp"

#include "constants.h"

using namespace std;

// -- Static helpers -- //

static void glfw_error_callback(int error, const char* description)
{
	fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

// Tries to find a translation file for the current system locale, return false on failure
static bool LoadLocaleTranslation()
{
	char name[LOCALE_NAME_MAX_LENGTH];
	{
		wchar_t w_name[LOCALE_NAME_MAX_LENGTH];
		if (LCIDToLocaleName(GetThreadLocale(), w_name, LOCALE_NAME_MAX_LENGTH, 0) == 0) {
			return false;
		}

		size_t retval = 0;
		if (wcstombs_s(&retval, name, LOCALE_NAME_MAX_LENGTH, w_name, LOCALE_NAME_MAX_LENGTH - 1) != 0) {
			return false;
		}
	}

	const uint64_t filenameMaxSize = LOCALE_NAME_MAX_LENGTH + 5 + sizeof(PROGRAM_LANG_FOLDER);
	char filename[filenameMaxSize];

	strcpy_s(filename, filenameMaxSize, PROGRAM_LANG_FOLDER);
	strcat_s(filename, filenameMaxSize, "/");
	strcat_s(filename, filenameMaxSize, name);
	strcat_s(filename, filenameMaxSize, ".txt");

	struct stat buffer;
	if (stat(filename, &buffer) == 0) {
		Localization::LoadFile(name);
		return true;
	}

	return false;
}

// Initialize the important members of mainwindow. I prefer doing it here because it is mostly a layout and GUI-related class
static void InitMainClasses(MainWindow& program)
{
	program.storage.ReloadMovesetList();

	GameAddressesFile* addrFile = new GameAddressesFile();

	program.extractor.Init(addrFile, &program.storage);
	program.importer.Init(addrFile, &program.storage);
	program.editionMenu.importer.Init(addrFile, &program.storage);

	if (Games::GetExtractableGamesCount() == 1) {
		program.extractor.SetTargetProcess(Games::GetGameInfo(0)->processName, 0);
		// todo : remove this when we implement more extractors. or maybe make T7 the default period?
	}

	if (Games::GetImportableGamesCount() == 1) {
		program.importer.SetTargetProcess(Games::GetGameInfo(0)->processName, 0);
		// todo : remove this when we implement more importers. or maybe make T7 the default period?
	}

	program.storage.StartThread();
	program.extractor.StartThread();
	program.importer.StartThread();
	// Separate importer for editor, less prone to bugs
	program.editionMenu.importer.StartThread();
}

// Free up memory and stop threads cleanly before exiting the program
static void DestroyMainClasses(MainWindow& program)
{
	// We allocated addrFile ocec even though multiple classes have a reference to it.
	GameAddressesFile* addrFile = program.importer.game->addrFile;

	// todo: Clean this up, we don't need to instantiate multiple GameData classes.
	// Maybe do the delete of .process inside StopThreadAndCleanup() as well
	program.extractor.StopThreadAndCleanup();
	delete program.extractor.process;
	delete program.extractor.game;

	program.importer.StopThreadAndCleanup();
	delete program.importer.process;
	delete program.importer.game;

	program.editionMenu.importer.StopThreadAndCleanup();
	delete program.editionMenu.importer.process;
	delete program.editionMenu.importer.game;

	// Now that every thread that uses addrFile has stopped, we can free it
	delete addrFile;

	// Once every thread that may use storage has been stopped, we can finally stop storage
	program.storage.StopThreadAndCleanup();


}

// -- main -- //

int main(int argc, wchar_t** argv, char** env)
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
	if (!LoadLocaleTranslation()) {
		Localization::LoadFile(PROGRAM_DEFAULT_LANG);
	}

	// Init main program. This will get most things going and create the important threads
	MainWindow program(window, c_glsl_version);
	InitMainClasses(program);

	while (!glfwWindowShouldClose(window)) {
		// Poll and handle events such as MKB inputs, window resize. Required
		glfwPollEvents();
		
		// Set window BG
		glClearColor(0.0f, 0.0f, 0.0f, 1.00f);
		glClear(GL_COLOR_BUFFER_BIT);

		// Probably framebuffer related? Required
		program.NewFrame();
		
		// Main layout function : every we display is in there
		program.Update();

		// Finishing touch. There shouldn't be any reason to ever touch these.
		program.Render();
		glfwSwapBuffers(window);
	}

	DestroyMainClasses(program);
	// Cleanup what needs to be cleaned up
	program.Shutdown();

	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}
