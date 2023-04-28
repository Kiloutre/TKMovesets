#include <string>
#include <windows.h>
#include <filesystem>

#include "GameProcess.hpp"

#include "constants.h"

// --  -- //

void StartProcess(const std::string& file)
{
	DEBUG_LOG("StartProcess '%s'\n", file.c_str());
	// Start process
	STARTUPINFO si;
	PROCESS_INFORMATION pi;

	// set the size of the structures
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	ZeroMemory(&pi, sizeof(pi));

	// start the program up
	CreateProcess(file.c_str(),   // the path
		NULL,        // Command line
		NULL,           // Process handle not inheritable
		NULL,           // Thread handle not inheritable
		FALSE,          // Set handle inheritance to FALSE
		0,              // No creation flags
		NULL,           // Use parent's environment block
		NULL,           // Use parent's starting directory 
		&si,            // Pointer to STARTUPINFO structure
		&pi             // Pointer to PROCESS_INFORMATION structure (removed extra parentheses)
	);
	// Close process and thread handles. 
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
}

void CleanupUpdateFiles(const std::string& filename)
{
	DEBUG_LOG("CleanupUpdateFiles()\n");
	// Wait until [filename] process has ended
	{
		// Max is 200 * 25 = 5 secs
		bool found = true;
		for (int i = 0; found && i < 25; ++i)
		{
			found = false;
			for (auto& process : GameProcessUtils::GetRunningProcessList())
			{
				DEBUG_LOG("- %s\n", process.name.c_str());
				if (process.name == filename) {
					DEBUG_LOG("Found [%s]\n", filename.c_str());
					found = true;
					break;
				}
			}
			if (found) {
				Sleep(200);
			}
			else {
				DEBUG_LOG("Not found.\n");
			}
		}

		if (found) {
			// timeout, don't do anything
			return;
		}
	}
	DEBUG_LOG("CleanupUpdateFiles() - Removing file\n");

	std::filesystem::remove(filename);
}

// Copy self .exe into TKMovesets.exe
void ApplyUpdate(const std::string& filename)
{
	DEBUG_LOG("ApplyUpdate()\n");
	// Wait until [UPDATE_FINALFILENAME] process has ended
	{
		// Max is 200 * 25 = 5 secs
		bool found = true;
		for (int i = 0; found && i < 25; ++i)
		{
			found = false;
			for (auto& process : GameProcessUtils::GetRunningProcessList())
			{
				DEBUG_LOG("- %s\n", process.name.c_str());
				if (process.name == PROGRAM_FILENAME) {
					DEBUG_LOG("Found [" PROGRAM_FILENAME "]\n");
					found = true;
					break;
				}
			}
			if (found) {
				Sleep(200);
			}
			else {
				DEBUG_LOG("Not found.\n");
			}
		}

		if (found) {
			// timeout, don't do anything
			return;
		}
	}
	DEBUG_LOG("ApplyUpdate() - Copying file\n");
	std::filesystem::remove(PROGRAM_FILENAME);
	std::filesystem::copy_file(filename, PROGRAM_FILENAME);

	StartProcess(".\\" PROGRAM_FILENAME);
}
