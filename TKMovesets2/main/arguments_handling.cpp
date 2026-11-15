#include <memory>
#include <windows.h>
#include <filesystem>
#include <fstream>

#include "Helpers.hpp"
#include "GameProcess.hpp"
#include "AnimExtractors.hpp"
#include "MovesetFolders.hpp"
#include "OpenRequests.hpp"
#include "Settings.hpp"

#include "constants.h"

// -- main -- //

static void MoveMovesetFile(const std::wstring& argFile)
{
	std::filesystem::path file(argFile);
	DEBUG_LOG("Argument: '%S'\n", argFile.c_str());

	if (!std::filesystem::exists(file)) {
		DEBUG_ERR("File does not exist: '%S'", argFile.c_str());
		return;
	}

	std::filesystem::path filename = file.filename();
	std::wstring extension = file.filename().extension().wstring();
	std::wstring name = file.filename().replace_extension("").wstring();

	DEBUG_LOG("Filename is '%S', name is '%S', ext is '%S'\n", filename.wstring().c_str(), name.c_str(), extension.c_str());

	if (extension == L"" MOVESET_FILENAME_EXTENSION)
	{
		// Moveset file : Move to "extracted_chars" folder
		std::filesystem::path moveset_dir = std::filesystem::current_path() / std::filesystem::path(MOVESET_DIRECTORY);
		std::filesystem::path target = moveset_dir / file.filename();

		// Append (N) to filename if necessary
		if (std::filesystem::exists(target))
		{
			DEBUG_LOG("File already exists\n");
			unsigned int number = 1;
			while (std::filesystem::exists(target))
			{
				++number;
				target = moveset_dir / std::filesystem::path(name + L" (" + std::to_wstring(number) + L")" MOVESET_FILENAME_EXTENSION);
			}
		}

		CreateDirectoryW(L"" MOVESET_DIRECTORY, nullptr);
		DEBUG_LOG("Copying to target '%S'\n", target.wstring().c_str());
		std::filesystem::copy_file(file, target);
		try {
			std::filesystem::remove(file);
		}
		catch (std::filesystem::filesystem_error const&) {
			DEBUG_LOG("Deletion of moveset '%S' after copy failed.\n", file.wstring().c_str());
		}
	}
	else {
		DEBUG_LOG("Invalid extension: '%S'\n", file.extension().wstring().c_str());
	}
}

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
		// Moveset already in a folder the moveset list reads: open it where it is instead of moving it
		std::wstring openRequest;
		if (first_arg == L"--move-moveset") {
			if (argc >= 3) {
				try {
					const std::filesystem::path file(argv[2]);
					Settings::LoadFile();
					MovesetFolders::Load();
					std::error_code error;
					if (file.extension() == L"" MOVESET_FILENAME_EXTENSION && std::filesystem::is_regular_file(file, error)
						&& MovesetFolders::IsInSearchDirectory(file)) {
						openRequest = std::filesystem::weakly_canonical(file).wstring();
					}
					else MoveMovesetFile(argv[2]);
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
