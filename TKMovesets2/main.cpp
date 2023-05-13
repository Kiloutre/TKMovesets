#ifdef BUILD_TYPE_DEBUG
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#include <stdlib.h>
#endif

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#define SDL_MAIN_HANDLED
#include <stdio.h>
#include <SDL.h>
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <SDL_opengles2.h>
#else
#include <SDL_opengl.h>
#endif

#include <stdio.h>
#include <format>
#include <windows.h>
#include <locale>
#include <filesystem>
#include <fstream>
#include <lz4.h>

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
	DEBUG_LOG("!! GLFW Error %d: '%s' !!\n", error, description);
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

	program.navMenu.requestedUpdatePtr = &program.requestedUpdate;
	program.navMenu.SetAddrFile(addrFile);

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

			if (!attachedImporter && gameInfo->extractor != nullptr) {
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

	for (EditorWindow* win : program.editorWindows) {
		delete win;
	}

	// Now that every thread that uses addrFile has stopped, we can free it
	delete program.addrFile;

	// Once every thread that may use storage has been stopped, we can finally stop storage
	program.storage.StopThreadAndCleanup();
}

static bool LoadEmbeddedIcon(SDL_Window* window)
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

	SDL_RWops* rwOps = SDL_RWFromConstMem(iconData, iconSize);
	auto iconSurface  = SDL_CreateRGBSurfaceFrom(iconData, 256, -256, 32, 256 * 4, 0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);

	SDL_SetWindowIcon(window, iconSurface);
	return true;
}

void StartProcess(const std::string& file);
void ApplyUpdate(const std::string& filename);
void CleanupUpdateFiles(const std::string& filename);

// -- main -- //

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

		std::string update_file_name = std::string(UPDATE_TMP_FILENAME) + ".exe";
		if (filename == update_file_name) {
			// Copy self .exe into TKMovesets.exe
			ApplyUpdate(update_file_name);
			return 0;
		}
		else if (Helpers::fileExists(update_file_name.c_str())) {
			CleanupUpdateFiles(update_file_name);
		}
	}

	SDL_SetMainReady();
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
	{
		printf("Error: %s\n", SDL_GetError());
		return -1;
	}

	// Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
	// GL ES 2.0 + GLSL 100
	const char* glsl_version = "#version 100";
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#elif defined(__APPLE__)
	// GL 3.2 Core + GLSL 150
	const char* glsl_version = "#version 150";
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG); // Always required on Mac
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#else
	// GL 3.0 + GLSL 130
	const char* glsl_version = "#version 130";
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif
	// From 2.0.18: Enable native IME.
#ifdef SDL_HINT_IME_SHOW_UI
	SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
#endif

	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
	SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
#ifdef BUILD_TYPE_DEBUG
	const char* windowTitle = PROGRAM_TITLE " " PROGRAM_VERSION " - DEBUG";
#else
	const char* windowTitle = PROGRAM_TITLE " " PROGRAM_VERSION;
#endif
	SDL_Window* window = SDL_CreateWindow(PROGRAM_TITLE " " PROGRAM_VERSION, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, PROGRAM_WIN_WIDTH, PROGRAM_WIN_HEIGHT, window_flags);
	SDL_GLContext gl_context = SDL_GL_CreateContext(window);
	SDL_GL_MakeCurrent(window, gl_context);
	SDL_GL_SetSwapInterval(1); // Enable vsync

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGuiIO& io = ImGui::GetIO();

	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; 
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

	if (!LoadEmbeddedIcon(window)) {
		DEBUG_LOG("Failed to load icon\n");
	}

	// Load translation
	if (!LoadLocaleTranslation()) {
		Localization::LoadFile(PROGRAM_DEFAULT_LOCALE);
	}

	{
		// Init main program. This will get most things going and create the important threads
		MainWindow program;
		InitMainClasses(program);

		ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
		ImGui_ImplOpenGL3_Init(glsl_version);

		bool done = false;
		ImVec4 clear_color = ImVec4(0.0f, 0.0f, 0.00f, 1.00f);

		while (!done && !program.requestedUpdate)
		{
			// Poll and handle events such as MKB inputs, window resize. Required
			SDL_Event event;
			while (SDL_PollEvent(&event))
			{
				ImGui_ImplSDL2_ProcessEvent(&event);
				if (event.type == SDL_QUIT)
					done = true;
				if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window))
					done = true;
			}

			// Probably framebuffer related? Required
			program.NewFrame();

			// Main layout function : everything we display is in there
			program.Update();

			// Let imgui do its stuff
			ImGui::Render();
			glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
			glClearColor(clear_color.x* clear_color.w, clear_color.y* clear_color.w, clear_color.z* clear_color.w, clear_color.w);
			glClear(GL_COLOR_BUFFER_BIT);
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

			// Update and Render additional Platform Windows
			// (Platform functions may change the current OpenGL context, so we save/restore it to make it easier to paste this code elsewhere.
			//  For this specific demo app we could also call SDL_GL_MakeCurrent(window, gl_context) directly)
			if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
			{
				SDL_Window* backup_current_window = SDL_GL_GetCurrentWindow();
				SDL_GLContext backup_current_context = SDL_GL_GetCurrentContext();
				ImGui::UpdatePlatformWindows();
				ImGui::RenderPlatformWindowsDefault();
				SDL_GL_MakeCurrent(backup_current_window, backup_current_context);
			}

			SDL_GL_SwapWindow(window);
		}


		DestroyMainClasses(program);
		// Cleanup what needs to be cleaned up
		program.Shutdown();

		if (program.requestedUpdate) {
			program.navMenu.CleanupThread();
			StartProcess(std::string(UPDATE_TMP_FILENAME) + ".exe");
		}
	}

	SDL_GL_DeleteContext(gl_context);
	SDL_DestroyWindow(window);
	SDL_Quit();

	Localization::Clear();


#ifdef BUILD_TYPE_DEBUG
	_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_DEBUG);
	_CrtDumpMemoryLeaks();
#endif

	return MAIN_ERR_NO_ERR;
}
