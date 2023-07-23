﻿#include "glad/glad.h"
#include "GLFW/glfw3.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <stdio.h>
#include <windows.h>

#include "MainWindow.hpp"
#include "Localization.hpp"
#include "GameAddressesFile.hpp"
#include "Settings.hpp"

#include "constants.h"

// From UpdateApply.cpp
void StartProcess(const std::string& file);

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
	if (Localization::LoadFile(name)) {
		Settings::Set(SETTING_LANG_KEY, Localization::GetCurrLangId());
		return true;
	}
	return false;
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
			processEntry* p;

			// Detect if the game is running
			{
				bool isRunning = false;
				for (auto& process : processList)
				{
					if (process.name == processName)
					{
						p = &process;
						isRunning = true;
						break;
					}
				}
				if (!isRunning) {
					continue;
				}
			}

			if (!gameInfo->MatchesProcessWindowName(p->pid)) {
				continue;
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


void run_gui()
{

	// Initialize GLFW library
	glfwSetErrorCallback(glfw_error_callback);
	if (!glfwInit()) {
		DEBUG_ERR("MAIN_ERR_GLFW_INIT");
		return;
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
		DEBUG_ERR("MAIN_ERR_WINDOW_CREATION");
		return;
	}

	// Set window for current thread
	glfwMakeContextCurrent(window);
	// Enable vsync
	glfwSwapInterval(Settings::Get(SETTING_VSYNC_BUFFER_KEY, SETTING_VSYNC_BUFFER));

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
		DEBUG_ERR("MAIN_ERR_OPENGL_CONTEXT");
		return;
	}

	if (!LoadEmbeddedIcon(window)) {
		DEBUG_LOG("Failed to load icon\n");
	}

	{
		// Set viewport for OpenGL
		int screen_width, screen_height;
		glfwGetFramebufferSize(window, &screen_width, &screen_height);
		glViewport(0, 0, screen_width, screen_height);
	}

	// Load settings
	Settings::LoadFile();

	// Load translation
	{
		int langId = Settings::Get(SETTING_LANG_KEY, SETTING_LANG);
		if (langId != -1) {
			// Attempt to load locale from settings
			if (!Localization::LoadFile(langId)) {
				Localization::LoadFile(PROGRAM_DEFAULT_LOCALE);
			}
		}
		else if (!LoadLocaleTranslation()) {
			// Attempted to load locale from automatic locale detection, failed
			Localization::LoadFile(PROGRAM_DEFAULT_LOCALE);
		}
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
			glClear(GL_COLOR_BUFFER_BIT);
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

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

		// Cleanup what needs to be cleaned up

		if (program.requestedUpdate) {
			program.sideMenu.CleanupThread();
			StartProcess(UPDATE_TMP_FILENAME ".exe");
		}

		DestroyMainClasses(program);
		program.Shutdown();
	}

	glfwDestroyWindow(window);
	glfwTerminate();

	Localization::Clear();
}