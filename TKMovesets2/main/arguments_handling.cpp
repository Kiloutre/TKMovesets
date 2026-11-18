#include <memory>
#include <windows.h>
#include <filesystem>
#include <fstream>

#include "Helpers.hpp"
#include "GameProcess.hpp"
#include "AnimExtractors.hpp"
#include "OpenRequests.hpp"

#include "constants.h"

// -- main -- //

static void MoveAnimationFile(const std::wstring& argument)
{
    namespace fs = std::filesystem;
    const fs::path source(argument);
    const auto extension = source.extension().wstring();
    if (extension != L"" ANIMATION_EXTENSION L"64" && extension != L"" ANIMATION_EXTENSION L"C8") return;
    if (!fs::is_regular_file(source)) return;
    const fs::path directory = fs::path(EDITOR_LIB_DIRECTORY) / "EXTERNAL";
    fs::create_directories(directory);
    auto destination = directory / source.filename();
    std::error_code error;
    if (fs::equivalent(source, destination, error)) return;
    for (unsigned number = 2; fs::exists(destination); ++number)
        destination = directory / (source.stem().wstring() + L" (" + std::to_wstring(number) + L")" + extension);
    // Preserve the original until a complete copy has succeeded.
    if (!fs::copy_file(source, destination)) return;
    // The library regenerates its metadata cache when it is absent.
    fs::remove(directory / "anims.txt", error);
    fs::remove(source, error);
}

bool handle_arguments()
{
	auto cmdLine = GetCommandLineW();
	int argc;
	auto argv = CommandLineToArgvW(cmdLine, &argc);
	if (!argv) return true;
	std::unique_ptr<void, decltype(&LocalFree)> owner(argv, LocalFree);

	if (argc != 1)
	{
		std::wstring first_arg = argv[1];
		// Moveset file: open it in the editor where it is, never move it
		// (--move-moveset is what associations made by older versions still pass)
		std::wstring openRequest;
		if (first_arg == L"--open-moveset" || first_arg == L"--move-moveset") {
			if (argc >= 3) {
				try {
					const std::filesystem::path file(argv[2]);
					std::error_code error;
					if (file.extension() == L"" MOVESET_FILENAME_EXTENSION && std::filesystem::is_regular_file(file, error)) {
						openRequest = std::filesystem::weakly_canonical(file).wstring();
					}
					else DEBUG_ERR("Not a moveset file: '%S'", argv[2]);
				} catch (const std::exception& error) { DEBUG_ERR("%s", error.what()); }
			}
		}
		else if (first_arg == L"--move-animation") {
			if (argc >= 3) {
				try { MoveAnimationFile(argv[2]); } catch (const std::exception& error) { DEBUG_ERR("%s", error.what()); }
			}
		}

		DWORD currPid = GetCurrentProcessId();
		for (auto& process : GameProcessUtils::GetRunningProcessList())
		{
			if (process.name == PROGRAM_FILENAME && process.pid != currPid) {
				DEBUG_LOG("Not starting another instance of this software if using args.\n");
				if (!openRequest.empty() && !OpenRequests::SendToInstance(process.pid, openRequest)) {
					DEBUG_ERR("Could not ask the running instance to open '%S'", openRequest.c_str());
				}
				return true;
			}
		}
		if (!openRequest.empty()) OpenRequests::Queue(openRequest);
	}
	return false;
}
