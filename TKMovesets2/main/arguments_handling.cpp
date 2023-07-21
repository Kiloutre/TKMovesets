#include <stdio.h>
#include <windows.h>
#include <filesystem>
#include <fstream>

#include "Helpers.hpp"
#include "GameProcess.hpp"
#include "AnimExtractors.hpp"

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

		if (moveset_dir == file.parent_path()) {
			DEBUG_LOG("Not copying moveset: already in our folders\n");
			return;
		}

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
			DEBUG_LOG("Deletion of moveset '%S' after copy failed.\n", file.string().c_str());
		}
	}
	else {
		DEBUG_LOG("Invalid extension: '%S'\n", file.extension().wstring().c_str());
	}
}

static void MoveAnimationFile(const std::wstring& argFile)
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

	if (extension == L"" ANIMATION_EXTENSION L"64" ||
		extension == L"" ANIMATION_EXTENSION L"C8")
	{
		// Animation file: move to animation library
		std::filesystem::path lib_dir = std::filesystem::current_path() / std::filesystem::path(EDITOR_LIB_DIRECTORY);
		std::filesystem::path output_folder = lib_dir / std::filesystem::path("EXTERNAL");

		CreateDirectoryW(L"" EDITOR_LIB_DIRECTORY, nullptr);
		CreateDirectoryW(output_folder.wstring().c_str(), nullptr);

		std::filesystem::path outputFile = output_folder / filename;
		std::filesystem::path animCache = output_folder / std::filesystem::path("anims.txt");

		if (output_folder == file.parent_path()) {
			DEBUG_LOG("Not copying anim file: already in our folders\n");
			return;
		}

		// Append (N) to filename if necessary
		if (std::filesystem::exists(outputFile))
		{
			unsigned int number = 1;
			while (std::filesystem::exists(outputFile))
			{
				++number;
				outputFile = output_folder / std::filesystem::path(name + L" (" + std::to_wstring(number) + L")" + extension);
			}
		}

		if (std::filesystem::exists(animCache))
		{
			Byte* anim;
			uint64_t s_anim;

			try {
				std::ifstream animationData(argFile, std::ios::binary);

				if (animationData.fail()) {
					DEBUG_ERR("Failed to analyze animation data");
					return;
				}

				animationData.seekg(0, std::ios::end);
				s_anim = animationData.tellg();
				anim = new Byte[s_anim];
				animationData.seekg(0, std::ios::beg);
				animationData.read((char*)anim, s_anim);
			}
			catch (const std::bad_alloc&)
			{
				DEBUG_ERR("Failed to allocate %llu bytes to analyze animation data", s_anim);
				return;
			}

			std::ofstream animCacheFile(animCache, std::ios::app);
			if (animCacheFile.fail()) {
				try {
					std::filesystem::remove(animCache);
				}
				catch (std::filesystem::filesystem_error const&) {
					DEBUG_LOG("Deletion of cache '%S' failed.\n", animCache.wstring().c_str());
				}
			}
			else {
				DEBUG_LOG("Copying to target '%S'..\n", outputFile.c_str());
				TAnimExtractorUtils::ExtractAnimation(Helpers::to_utf8(name).c_str(), anim, output_folder, animCacheFile, L"" ANIMATION_EXTENSION);
			}

			delete[] anim;
		}
		else
		{
			DEBUG_LOG("Copying to target '%S'\n", outputFile.wstring().c_str());
			std::filesystem::copy_file(argFile, outputFile);
		}

		try {
			std::filesystem::remove(argFile);
		}
		catch (std::filesystem::filesystem_error const&) {
			DEBUG_LOG("Deletion of animation '%S' after copy failed.\n", argFile.c_str());
		}
	}
}

bool handle_arguments()
{
	auto cmdLine = GetCommandLineW();
	int argc;
	auto argv = CommandLineToArgvW(cmdLine, &argc);

	if (argc != 1)
	{
		std::wstring first_arg = argv[1];
		if (first_arg == L"--move-moveset") {
			if (argc >= 2) {
				MoveMovesetFile(argv[2]);
			}
		}
		else if (first_arg == L"--move-animation") {
			if (argc >= 2) {
				MoveAnimationFile(argv[2]);
			}
		}

		DWORD currPid = GetCurrentProcessId();
		for (auto& process : GameProcessUtils::GetRunningProcessList())
		{
			if (process.name == PROGRAM_FILENAME && process.pid != currPid) {
				DEBUG_LOG("Not starting another instance of this software if using args.\n");
				return true;
			}
		}
	}
	return false;
}
