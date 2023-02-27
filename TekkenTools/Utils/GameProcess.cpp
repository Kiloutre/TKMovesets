#include <chrono>
#include <thread>
#include <windows.h>
#include <tlhelp32.h>

#include "GameProcess.h"

// Starts the attaching process in an existing other thread
void GameProcess::Attach()
{
	if (errcode == PROC_ATTACHED)
		throw("Process is already attached : not re-attaching.");

	processName = "TekkenGame-Win64-Shipping.exe";
	if (!threadStarted)
	{
		threadStarted = true;
		std::thread t(&GameProcess::Update, this);
		t.detach();
	}
}

// The idea here is to use functions that require less privilege than OpenProcess, but i believe right now there is no difference
DWORD GameProcess::GetGamePID()
{
	HANDLE hProcessSnap;
	PROCESSENTRY32 pe32;
	hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

	if (GetLastError() == ERROR_ACCESS_DENIED) return (DWORD)-1;

	pe32.dwSize = sizeof(PROCESSENTRY32);
	if (Process32First(hProcessSnap, &pe32)) {
		if (strcmp(pe32.szExeFile, processName.c_str()) == 0) {
			DWORD pid = pe32.th32ProcessID;
			CloseHandle(hProcessSnap);
			return pid;
		}

		while (Process32Next(hProcessSnap, &pe32)) {
			if (strcmp(pe32.szExeFile, processName.c_str()) == 0) {
				DWORD pid = pe32.th32ProcessID;
				CloseHandle(hProcessSnap);
				return pid;
			}
		}
		CloseHandle(hProcessSnap);
	}

	return (DWORD)-1;
}

void GameProcess::AttachToNamedProcess()
{
	DWORD pid = GetGamePID();

	if (pid == (DWORD)-1) errcode = PROC_NOT_FOUND;
	else
	{
		hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
		if (hProcess != NULL) errcode = (hProcess != NULL) ? PROC_ATTACHED : PROC_ATTACH_ERR;
	}
}

void GameProcess::DetachFromGame()
{
	if (hProcess != NULL) {
		CloseHandle(hProcess);
		hProcess = NULL;
	}
	errcode = PROC_NOT_ATTACHED;
}

void GameProcess::Update()
{
	errcode = PROC_ATTACHING;
	while (threadStarted)
	{
		if (errcode == PROC_ATTACHING) AttachToNamedProcess();
		else if (errcode == PROC_ATTACHED)
		{
			// Todo: try reading from a random value, see if an exception is thrown
			if (GetGamePID() == (DWORD)-1) {
				hProcess = NULL;
				errcode = PROC_EXITED;
			}
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(500));
	}
	DetachFromGame();
}