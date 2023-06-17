﻿#ifdef BUILD_TYPE_DEBUG
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#include <stdlib.h>
#endif

#include "glad/glad.h"
#include "GLFW/glfw3.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <stdio.h>
#include <format>
#include <windows.h>
#include <locale>
#include <filesystem>
#include <fstream>
#include <windows.h>

#include "MainWindow.hpp"
#include "Localization.hpp"
#include "GameAddressesFile.hpp"

#include "constants.h"

// -- Errcodes -- //
# define MAIN_ERR_GLFW_INIT         (1)
# define MAIN_ERR_WINDOW_CREATION   (2)
# define MAIN_ERR_OPENGL_CONTEXT    (3)
# define MAIN_ERR_MOVESET_LOADER    (4)
# define MAIN_ERR_NO_MOVESET_LOADER (5)
# define MAIN_ERR_NO_ERR            (0)

// -- Static helpers -- //

static void glfw_error_callback(int error, const char* description)
{
	DEBUG_ERR("GLFW Error %d: '%s'", error, description);
}

// Tries to find a translation file for the current system locale, return false on failure
static bool LoadLocaleTranslation()
{
	char name[LOCALE_NAME_MAX_LENGTH];
	wchar_t w_name[LOCALE_NAME_MAX_LENGTH];
	{
		if (LCIDToLocaleName(GetThreadLocale(), w_name, LOCALE_NAME_MAX_LENGTH, 0) == 0) {
			return false;
		}

		size_t retval = 0;
		if (wcstombs_s(&retval, name, LOCALE_NAME_MAX_LENGTH, w_name, LOCALE_NAME_MAX_LENGTH - 1) != 0) {
			return false;
		}
	}

#ifdef BUILD_TYPE_DEBUG
	std::ofstream f("TKM-DEBUG-locale.txt");
	auto t = std::format("[{}]\n", name);
	f.write(t.c_str(), t.size());
	f.write((char*)w_name, wcslen(w_name) * sizeof(wchar_t));
#endif
	DEBUG_LOG("Attempting to load locale %s\n", name);
	return Localization::LoadFile(name);
}

// Initialize the important members of mainwindow. I prefer doing it here because it is mostly a layout and GUI-related class
static void InitMainClasses(MainWindow& program)
{
	program.storage.ReloadMovesetList();

	GameAddressesFile* addrFile = new GameAddressesFile(true);
	program.addrFile = addrFile;

	program.extractor.Init(addrFile, &program.storage);
	program.importer.Init(addrFile, &program.storage);
	program.sharedMem.Init(addrFile, &program.storage);

	program.onlineMenu.gameHelper = &program.sharedMem;
	program.persistentPlayMenu.gameHelper = &program.sharedMem;

	program.sideMenu.requestedUpdatePtr = &program.requestedUpdate;
	program.sideMenu.SetAddrFile(addrFile);

	{
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

			// Detect if the game is running
			{
				bool isRunning = false;
				for (auto& process : processList)
				{
					if (process.name == processName)
					{
						isRunning = true;
						break;
					}
				}
				if (!isRunning) {
					continue;
				}
			}

			if (!attachedExtractor && gameInfo->extractor != nullptr) {
				program.extractor.SetTargetProcess(gameInfo);
				attachedExtractor = true;
				DEBUG_LOG("Extraction-compatible game '%s' already running: attaching.\n", processName);
			}

			if (!attachedImporter && gameInfo->importer != nullptr) {
				program.importer.SetTargetProcess(gameInfo);
				attachedImporter = true;
				DEBUG_LOG("Importation-compatible game '%s' already running: attaching.\n", processName);
			}

			if (!attachedOnline && gameInfo->onlineHandler != nullptr) {
				program.sharedMem.SetTargetProcess(gameInfo);
				attachedOnline = true;
				DEBUG_LOG("Online-compatible game '%s' already running: attaching.\n", processName);
			}

		}
	}

	program.storage.StartThread();
	program.extractor.StartThread();
	program.importer.StartThread();
	program.sharedMem.StartThread();
}

// Free up memory and stop threads cleanly before exiting the program
static void DestroyMainClasses(MainWindow& program)
{
	program.extractor.StopThreadAndCleanup();
	program.importer.StopThreadAndCleanup();
	program.sharedMem.StopThreadAndCleanup();

	for (auto& win : program.editorWindows) {
		delete win;
	}

	// Now that every thread that uses addrFile has stopped, we can free it
	delete program.addrFile;

	// Once every thread that may use storage has been stopped, we can finally stop storage
	program.storage.StopThreadAndCleanup();
}

static bool LoadEmbeddedIcon(GLFWwindow* window)
{
	HMODULE hInstance = GetModuleHandle(NULL);

	HRSRC hResource = FindResource(hInstance, MAKEINTRESOURCE(1), RT_ICON);
	if (hResource == nullptr) {
		return false;
	}
	HGLOBAL hResourceData = LoadResource(hInstance, hResource);
	if (hResourceData == nullptr) {
		return false;
	}

	DWORD iconSize = SizeofResource(hInstance, hResource);
	LPVOID iconData = LockResource(hResourceData);

	// Create an HICON object from the resource data
	HICON hIcon = CreateIconFromResourceEx((PBYTE)iconData, iconSize, TRUE, 0x00030000, 0, 0, LR_DEFAULTCOLOR);

	// Convert the HICON object to a GLFWimage structure
	ICONINFO iconInfo;
	GetIconInfo(hIcon, &iconInfo);

	BITMAPINFO bitmapInfo;
	ZeroMemory(&bitmapInfo, sizeof(BITMAPINFO));
	bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bitmapInfo.bmiHeader.biWidth = iconInfo.xHotspot * 2;
	bitmapInfo.bmiHeader.biHeight = -((signed)iconInfo.yHotspot * 2);
	bitmapInfo.bmiHeader.biPlanes = 1;
	bitmapInfo.bmiHeader.biBitCount = 32;

	uint64_t allocSize = bitmapInfo.bmiHeader.biWidth * (uint64_t)abs(bitmapInfo.bmiHeader.biHeight) * 4;
	BYTE* bitmapData = new BYTE[allocSize];
	ZeroMemory(bitmapData, allocSize);

	GetDIBits(GetDC(NULL), iconInfo.hbmColor, 0, iconInfo.yHotspot * 2, bitmapData, &bitmapInfo, DIB_RGB_COLORS);

	GLFWimage image{
		.width = (int)(iconInfo.xHotspot * 2),
		.height = (int)(iconInfo.yHotspot * 2),
		.pixels = bitmapData
	};

	// Set the window icon
	glfwSetWindowIcon(window, 1, &image);

	// Clean up
	DeleteObject(iconInfo.hbmColor);
	DeleteObject(iconInfo.hbmMask);
	DestroyIcon(hIcon);
	UnlockResource(iconData);
	FreeResource(hResourceData);

	return true;
}


void StartProcess(const std::string& file);
void ApplyUpdate(const std::string& filename);
void CleanupUpdateFiles(const std::string& filename);

// -- main -- //

static void HandleFileArgument(const std::wstring& argFile)
{
	DEBUG_LOG("Argument: '%S'\n", argFile.c_str());

	if (!Helpers::fileExists(argFile.c_str())) {
		DEBUG_ERR("File does not exist: '%S'", argFile.c_str());
	}
	else {
		std::wstring filename = argFile.substr(argFile.find_last_of(L"\\") + 1);
		std::wstring name = filename.substr(0, filename.find_last_of(L"."));

		DEBUG_LOG("Filename is '%S'\n", filename.c_str());

		if (Helpers::endsWith<std::wstring>(argFile, L"" MOVESET_FILENAME_EXTENSION))
		{
			// Moveset file : Move to "extracted_chars" folder
			std::wstring target = std::wstring(std::filesystem::current_path()) + L"\\extracted_chars\\" + filename;

			if (wcscmp(target.c_str(), argFile.c_str()) == 0) {
				DEBUG_LOG("Not copying moveset: already in our folders\n");
				return;
			}

			if (Helpers::fileExists(target.c_str()))
			{
				DEBUG_LOG("File already exists\n");
				const std::wstring prefix = std::wstring(std::filesystem::current_path()) + L"\\" MOVESET_DIRECTORY L"\\";
				
				unsigned int number = 2;
				target = prefix + name + L" (" + std::to_wstring(number) + L") " MOVESET_FILENAME_EXTENSION;
				while (Helpers::fileExists(target.c_str()))
				{
					++number;
					target = prefix + name + L" (" + std::to_wstring(number) + L")" MOVESET_FILENAME_EXTENSION;
				}
			}


			CreateDirectoryW(L"" MOVESET_DIRECTORY, nullptr);
			DEBUG_LOG("Copying to target '%S'..\n", target.c_str());
			std::filesystem::copy_file(argFile, target);
			try {
				std::filesystem::remove(argFile);
			}
			catch (std::filesystem::filesystem_error const&) {
				DEBUG_LOG("Deletion of moveset '%S' after copy failed.\n", argFile.c_str());
			}
		}
		else if (Helpers::endsWith<std::wstring>(argFile, L"" ANIMATION_EXTENSION L"64")
			|| Helpers::endsWith<std::wstring>(argFile, L"" ANIMATION_EXTENSION L"C8"))
		{
			// Animation file: move to animation library
			std::wstring prefix = std::wstring(std::filesystem::current_path()) + L"\\" EDITOR_LIB_DIRECTORY "\\";
			std::wstring outputFolder = prefix + L"EXTERNAL\\";

			CreateDirectoryW(L"" EDITOR_LIB_DIRECTORY, nullptr);
			CreateDirectoryW(outputFolder.c_str(), nullptr);

			std::wstring outputFile = outputFolder + filename;

			if (wcscmp(outputFile.c_str(), argFile.c_str()) == 0 || Helpers::startsWith<std::wstring>(argFile, prefix)) {
				DEBUG_LOG("Not copying anim file: already in our folders\n");
				return;
			}

			if (Helpers::fileExists(outputFile.c_str()))
			{
				unsigned int number = 2;
				std::wstring extension = filename.substr(filename.find_last_of(L"."));
				outputFile = outputFolder + name + L" (" + std::to_wstring(number) + L")" + extension;
				while (Helpers::fileExists(outputFile.c_str()))
				{
					++number;
					outputFile = outputFolder + name + L" (" + std::to_wstring(number) + L")" + extension;
				}
			}

			DEBUG_LOG("Copying to target '%S'..\n", outputFile.c_str());
			std::filesystem::copy_file(argFile, outputFile);
			try {
				std::filesystem::remove(argFile);
			}
			catch (std::filesystem::filesystem_error const&) {
				DEBUG_LOG("Deletion of moveset '%S' after copy failed.\n", argFile.c_str());
			}
		}
	}
}

int WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_  LPSTR lpCmdLine, _In_  int nShowCmd)
{
#ifdef BUILD_TYPE_DEBUG
	AllocConsole();
#pragma warning(push)
#pragma warning(disable:4996)
	freopen("CONOUT$", "w", stdout);
	freopen("CONOUT$", "w", stderr);
#pragma warning(pop)
#endif

	{
		std::wstring oldWorkingDir = std::filesystem::current_path().wstring();

		// Make sure working dir is same as .exe
		wchar_t currPath[MAX_PATH] = { 0 };
		GetModuleFileNameW(nullptr, currPath, MAX_PATH);
		std::wstring ws(currPath);
		std::string filename = Helpers::wstring_to_string(ws.substr(ws.find_last_of(L"\\") + 1));
		ws.erase(ws.find_last_of(L"\\"));

		if (ws != std::filesystem::current_path()) {
			std::filesystem::current_path(ws);
		}

		std::string update_file_name = UPDATE_TMP_FILENAME ".exe";
		if (filename == update_file_name) {
			// Copy self .exe into TKMovesets.exe
			ApplyUpdate(update_file_name);
			return 0;
		}
		else if (Helpers::fileExists(update_file_name.c_str())) {
			CleanupUpdateFiles(update_file_name);
		}
	}

	// Handle file arguments
	{
		auto cmdLine = GetCommandLineW();
		int argc;
		auto argv = CommandLineToArgvW(cmdLine, &argc);

		if (argc == 2) {
			HandleFileArgument(argv[1]);

			DWORD currPid = GetCurrentProcessId();
			for (auto& process : GameProcessUtils::GetRunningProcessList())
			{
				if (process.name == PROGRAM_FILENAME && process.pid != currPid) {
					DEBUG_LOG("Not starting another instance of this software if using args.\n");
					return 0;
				}
			}
		}
	}

	// Initialize GLFW library
	glfwSetErrorCallback(glfw_error_callback);
	if (glfwInit() == GLFW_FALSE) {
		return MAIN_ERR_GLFW_INIT;
	}

	// GL 3.0 + GLSL 130
	// Set the window hint. Not really that important to be honest.
	const char* c_glsl_version = "#version 130";
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
	//glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
	//glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only

	// Setup window title and create window
	GLFWwindow* window;
	{
#ifdef BUILD_TYPE_DEBUG
		const char* windowTitle = PROGRAM_TITLE " " PROGRAM_VERSION " - DEBUG";
#else
		const char* windowTitle = PROGRAM_TITLE " " PROGRAM_VERSION;
#endif
		window = glfwCreateWindow(PROGRAM_WIN_WIDTH, PROGRAM_WIN_HEIGHT, windowTitle, nullptr, nullptr);
	}

	if (window == nullptr) {
		return MAIN_ERR_WINDOW_CREATION;
	}

	// Set window for current thread
	glfwMakeContextCurrent(window);
	// Enable vsync
	glfwSwapInterval(VSYNC_BUFFER);

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGuiIO& io = ImGui::GetIO();
	io.IniFilename = nullptr; //I don't want to save settings (for now). Perhaps save in appdata later.
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	// Initialize backends
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init(c_glsl_version);

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
		DEBUG_LOG("Unable to context to OpenGL");
		return MAIN_ERR_OPENGL_CONTEXT;
	}

	if (!LoadEmbeddedIcon(window)) {
		DEBUG_LOG("Failed to load icon\n");
	}

	{
		// Set viewport for OpenGL
		int screen_width, screen_height;
		glfwGetFramebufferSize(window, &screen_width, &screen_height);
		glViewport(0, 0, screen_width, screen_height);

		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();

		//const float ar = (float)screen_width / (float)screen_height;
		//glFrustum(-ar, ar, -1.0, 1.0, 2.0, 100.0);
		glOrtho(-300, 300, -300, 300, -1, 1);

		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
	}

	// Load translation
	if (!LoadLocaleTranslation()) {
		Localization::LoadFile(PROGRAM_DEFAULT_LOCALE);
	}

	{
		// Init main program. This will get most things going and create the important threads
		MainWindow program;
		InitMainClasses(program);

		while (!glfwWindowShouldClose(window) && !program.requestedUpdate)
		{
			// Poll and handle events such as MKB inputs, window resize. Required
			glfwPollEvents();

			// Probably framebuffer related? Required
			program.NewFrame();

			// Main layout function : everything we display is in there
			program.Update();

			// Let imgui do its stuff
			ImGui::Render();
			{
				int display_w, display_h;
				glfwGetFramebufferSize(window, &display_w, &display_h);
				glViewport(0, 0, display_w, display_h);
			}
			glClearColor(0.0f, 0.0f, 0.0f, 1.00f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

			//glBegin(GL_QUADS);
			/*
			glColor3d(1, 0, 0);
			glVertex3f(-1, -1, -10);
			glColor3d(1, 1, 0);
			glVertex3f(1, -1, -10);
			glColor3d(1, 1, 1);
			glVertex3f(1, 1, -10);
			glColor3d(0, 1, 1);
			glVertex3f(-1, 1, -10);
			glColor3d(0, 1, 1);
			glVertex3f(-4, 4, -10);
			*/

			glColor3f(1.0f, 0.0f, 0.0f);
			glRectf(225.0f, 150.0f, 150.0f, 100.0f);
			//glEnd();

			

			
			if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
			{
				GLFWwindow* backup_current_context = glfwGetCurrentContext();
				ImGui::UpdatePlatformWindows();
				ImGui::RenderPlatformWindowsDefault();
				glfwMakeContextCurrent(backup_current_context);
			}
			
			glfwSwapBuffers(window);

			if (io.DeltaTime < (1.0f / MAX_FPS)) {
				Sleep((DWORD)(((1.0f / MAX_FPS) - io.DeltaTime) * 1000));
			}
		}

		DestroyMainClasses(program);
		// Cleanup what needs to be cleaned up
		program.Shutdown();

		if (program.requestedUpdate) {
			program.sideMenu.CleanupThread();
			StartProcess(UPDATE_TMP_FILENAME ".exe");
		}
	}

	glfwDestroyWindow(window);
	glfwTerminate();

	Localization::Clear();

#ifdef BUILD_TYPE_DEBUG
	FreeConsole();
	_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_DEBUG);
	_CrtDumpMemoryLeaks();
#endif

	return MAIN_ERR_NO_ERR;
}
