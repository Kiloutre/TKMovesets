#ifdef BUILD_TYPE_DEBUG
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#include <stdlib.h>
#endif

#include <imgui_impl_sdl2.h>
#include <imgui_impl_sdlrenderer.h>
#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>

#if !SDL_VERSION_ATLEAST(2,0,17)
#error This backend requires SDL 2.0.17+ because of SDL_RenderGeometry() function
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

static void WriteToLogFile(const std::string& content, bool append=true)
{
	DEBUG_LOG("%s\n", content.c_str());
	/*
	std::ofstream file;

	if (append) {
		file.open(PROGRAM_DEBUG_LOG_FILE, std::ios_base::app);
	}
	else {
		file.open(PROGRAM_DEBUG_LOG_FILE);
	}

	if (!file.fail()) {
		file.write(content.c_str(), content.size());
		file.write("\n", 1);
	}
	*/
}

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

	WriteToLogFile(std::format("Attempting to load locale {}", name));
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

/*
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

	uint64_t allocSize = bitmapInfo.bmiHeader.biWidth * (uint64_t) abs(bitmapInfo.bmiHeader.biHeight) * 4;
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
	return true;
}
*/

void StartProcess(const std::string& file);
void ApplyUpdate(const std::string& filename);
void CleanupUpdateFiles(const std::string& filename);

// -- main -- //

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
#ifdef BUILD_TYPE_DEBUG
	AllocConsole();
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
			WriteToLogFile(std::format("OLD CWD is {}", Helpers::wstring_to_string(oldWorkingDir)));
			WriteToLogFile(std::format("Set CWD to {}", Helpers::wstring_to_string(ws)));
		}

		WriteToLogFile("Started TKMovesets " PROGRAM_VERSION, false);

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
		return -1;
	}
	WriteToLogFile("Initiated");

	// From 2.0.18: Enable native IME.
#ifdef SDL_HINT_IME_SHOW_UI
	SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
#endif

	SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
	SDL_Window* window = SDL_CreateWindow(PROGRAM_TITLE " " PROGRAM_VERSION, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, PROGRAM_WIN_WIDTH, PROGRAM_WIN_HEIGHT, window_flags);
	SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED);
	WriteToLogFile("window created");

	if (renderer == nullptr ||window == nullptr) {
		return MAIN_ERR_WINDOW_CREATION;
	}

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGuiIO& io = ImGui::GetIO();

	//io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; 
	//io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;



	ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
	ImGui_ImplSDLRenderer_Init(renderer);

	/*
	if (!LoadEmbeddedIcon(window)) {
		DEBUG_LOG("Failed to load icon\n");
	}
	*/

	// Load translation
	if (!LoadLocaleTranslation()) {
		Localization::LoadFile(PROGRAM_DEFAULT_LOCALE);
	}

	{
		// Init main program. This will get most things going and create the important threads
		MainWindow program;
		InitMainClasses(program);

		WriteToLogFile("Initiated main classes");
		bool done = false;
		ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

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
			SDL_RenderSetScale(renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
			SDL_SetRenderDrawColor(renderer, (Uint8)(clear_color.x * 255), (Uint8)(clear_color.y * 255), (Uint8)(clear_color.z * 255), (Uint8)(clear_color.w * 255));
			SDL_RenderClear(renderer);
			ImGui_ImplSDLRenderer_RenderDrawData(ImGui::GetDrawData());
			SDL_RenderPresent(renderer);
		}


		DestroyMainClasses(program);
		WriteToLogFile("Destroyed main classes");
		// Cleanup what needs to be cleaned up
		program.Shutdown();
		WriteToLogFile("Shut down");

		if (program.requestedUpdate) {
			program.navMenu.CleanupThread();
			StartProcess(std::string(UPDATE_TMP_FILENAME) + ".exe");
		}
	}

	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();

	Localization::Clear();


#ifdef BUILD_TYPE_DEBUG
	_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_DEBUG);
	_CrtDumpMemoryLeaks();
#endif

	return MAIN_ERR_NO_ERR;
}
