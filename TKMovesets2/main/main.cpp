#ifdef BUILD_TYPE_DEBUG
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#include <stdlib.h>
#endif

#include <windows.h>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <vector>
#include "headless.hpp"

#include "main.hpp"
#include "Helpers.hpp"
#include "EmbeddedInterface.hpp"

#include "constants.h"

// -- Errcodes -- //
# define MAIN_ERR_NO_ERR (0)

// -- main -- //

// This is a GUI-subsystem program, so a console that starts it gives it no stdout.
// Attach to that console unless output is already redirected to a file or pipe.
static void AttachParentConsoleOutput()
{
	auto unattached = [](DWORD id) {
		const HANDLE handle = GetStdHandle(id);
		return handle == nullptr || handle == INVALID_HANDLE_VALUE || GetFileType(handle) == FILE_TYPE_UNKNOWN;
	};
	const bool out = unattached(STD_OUTPUT_HANDLE), err = unattached(STD_ERROR_HANDLE);
	if (!(out || err) || !AttachConsole(ATTACH_PARENT_PROCESS)) return;
	FILE* stream = nullptr;
	if (out) freopen_s(&stream, "CONOUT$", "w", stdout);
	if (err) freopen_s(&stream, "CONOUT$", "w", stderr);
	// Start below the shell prompt, which a non-waiting shell has already printed.
	if (out) std::fputs("\n", stdout);
}

int WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_  LPSTR lpCmdLine, _In_  int nShowCmd)
{
    // standalone exe: fighter names and editor labels are embedded (compressed) and unpacked to TKMInterface/
    EmbeddedInterface::Unpack();
    int argc = 0;
    auto argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) return 2;
    std::vector<std::wstring> arguments(argv + 1, argv + argc);
    LocalFree(argv);
    if (!arguments.empty() && arguments[0] == L"--headless") {
        AttachParentConsoleOutput();
        return RunHeadless(std::span(arguments).subspan(1));
    }
    if (arguments.size() == 1 && (arguments[0] == L"--help" || arguments[0] == L"-h" || arguments[0] == L"/?")) {
        AttachParentConsoleOutput();
        const std::wstring help = L"--help";
        return RunHeadless(std::span(&help, 1));
    }

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

	int result = 0;
	if (handle_arguments()) {
		// Do nothing
	}
	else {
		result = run_gui();
	}

#ifdef BUILD_TYPE_DEBUG
	FreeConsole();
	_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_DEBUG);
	_CrtDumpMemoryLeaks();
#endif

	return result;
}
