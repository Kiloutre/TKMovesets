#ifdef BUILD_TYPE_DEBUG
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#include <stdlib.h>
#endif

#include <windows.h>
#include <filesystem>

#include "main.hpp"
#include "Helpers.hpp"

#include "constants.h"

// -- Errcodes -- //
# define MAIN_ERR_NO_ERR (0)

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
		wchar_t currPath[MAX_PATH + 1] = { 0 };
		GetModuleFileNameW(nullptr, currPath, MAX_PATH);

		std::filesystem::path executable_path = std::filesystem::path(currPath);
		std::filesystem::path exe_directory = executable_path.parent_path();
		std::string basename = executable_path.filename().string();

		if (exe_directory != std::filesystem::current_path()) {
			std::filesystem::current_path(exe_directory);
			DEBUG_LOG("Not in correct directory, setting current dir as: %S\n", exe_directory.wstring().c_str());
		}

		std::string update_file_name = "" UPDATE_TMP_FILENAME ".exe";
		if (basename == update_file_name) {
			// Copy self .exe into TKMovesets.exe
			ApplyUpdate(update_file_name);
			return MAIN_ERR_NO_ERR;
		}
		else if (Helpers::fileExists(update_file_name.c_str())) {
			CleanupUpdateFiles(update_file_name);
		}
	}

	if (handle_arguments()) {
		// Do nothing
	}
	else {
		run_gui();
	}

#ifdef BUILD_TYPE_DEBUG
	FreeConsole();
	_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_DEBUG);
	_CrtDumpMemoryLeaks();
#endif

	return MAIN_ERR_NO_ERR;
}
