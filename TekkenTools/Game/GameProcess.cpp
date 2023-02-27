#include <chrono>
#include <thread>
#include <windows.h>
#include <tlhelp32.h>

#include "GameProcess.hpp"

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

	if (GetLastError() != ERROR_ACCESS_DENIED)
	{
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
		}
		CloseHandle(hProcessSnap);
	}

	return (DWORD)-1;
}

bool GameProcess::LoadGameMainModule(DWORD pid)
{
	HANDLE moduleSnap;
	MODULEENTRY32 me32 = { };

	moduleSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);

	if (GetLastError() != ERROR_ACCESS_DENIED)
	{
		me32.dwSize = sizeof(MODULEENTRY32);
		if (Module32First(moduleSnap, &me32)) {
			if (strcmp(me32.szModule, processName.c_str()) == 0) {
				modBaseAddr = me32.modBaseAddr;
				CloseHandle(moduleSnap);
				return true;
			}

			while (Module32Next(moduleSnap, &me32)) {
				if (strcmp(me32.szModule, processName.c_str()) == 0) {

					modBaseAddr = me32.modBaseAddr;
					CloseHandle(moduleSnap);
					return true;
				}
			}
		}
		CloseHandle(moduleSnap);
	}

	return false;
}

GameProcessError GameProcess::AttachToNamedProcess()
{
	DWORD pid = GetGamePID();

	if (pid == (DWORD)-1) return PROC_NOT_FOUND;
	else {
		hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
		if (hProcess != nullptr && LoadGameMainModule(pid)) {
			printf("Module addr: %p\n", modBaseAddr);
			processId = pid;
			return PROC_ATTACHED;
		}
		else {
			return PROC_ATTACH_ERR;
		}
	}
}

void GameProcess::DetachFromGame()
{
	if (hProcess != nullptr) {
		CloseHandle(hProcess);
		hProcess = nullptr;
	}
	errcode = PROC_NOT_ATTACHED;
}

void GameProcess::Update()
{
	errcode = PROC_ATTACHING;
	while (threadStarted)
	{
		if (errcode == PROC_ATTACHED)
		{
			// Todo: try reading from a random value, see if an exception is thrown
			if (GetGamePID() == (DWORD)-1) {
				hProcess = nullptr;
				errcode = PROC_EXITED;
			}
		} else if (errcode != PROC_ATTACHED)
			errcode = AttachToNamedProcess();

		std::this_thread::sleep_for(std::chrono::milliseconds(500));
	}
	DetachFromGame();
}

bool GameProcess::AttemptRead()
{
	/*
	try {
		//readInt(modBaseAddr);
	} catch() {
		return false;
	}
	*/
	return true;
}

char GameProcess::readByte(long addr)
{
	return 0;
}

short GameProcess::readShort(long addr)
{
	return 0;
}

int GameProcess::readInt(long addr)
{
	return 0;
}

float GameProcess::readFloat(long addr)
{
	return 0;
}

void GameProcess::readBytes(long addr, char* buf, size_t bufSize)
{

}

long GameProcess::allocateMem(size_t amount)
{
	return 0;
}

void GameProcess::freeMem(long addr)
{

}
