#include <chrono>
#include <thread>
#include <windows.h>
#include <tlhelp32.h>

#include "GameProcess.hpp"

// -- Private : Helpers -- //

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
	MODULEENTRY32 me32 = {};

	moduleSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);

	if (GetLastError() != ERROR_ACCESS_DENIED)
	{
		me32.dwSize = sizeof(MODULEENTRY32);
		if (Module32First(moduleSnap, &me32)) {
			if (strcmp(me32.szModule, processName.c_str()) == 0) {
				modBaseAddr = (int64_t)me32.modBaseAddr;
				CloseHandle(moduleSnap);
				return true;
			}

			while (Module32Next(moduleSnap, &me32)) {
				if (strcmp(me32.szModule, processName.c_str()) == 0) {

					modBaseAddr = (int64_t)me32.modBaseAddr;
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
			processId = pid;
			recentlyReloaded = true;
			return PROC_ATTACHED;
		}
		else {
			return PROC_ATTACH_ERR;
		}
	}
}

// -- Public : Lifespan -- //

// Starts the attaching process in an existing other thread
void GameProcess::StartAttachingThread()
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
			if (GetGamePID() == (DWORD)-1 || !GameProcess::AttemptRead()) {
				hProcess = nullptr;
				errcode = PROC_EXITED;
			}
		}
		else if (errcode != PROC_ATTACHED) {
			errcode = AttachToNamedProcess();
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(500));
	}
	DetachFromGame();
}

bool GameProcess::AttemptRead()
{
	// Todo
	/*
	try {
		//readInt32(modBaseAddr);
	} catch() {
		return false;
	}
	*/
	return true;
}

// -- Public: Other -- //

bool GameProcess::MustReloadAddresses()
{
	bool mustReload = recentlyReloaded;
	if (mustReload) recentlyReloaded = false;
	return mustReload;
}

// -- Public : Reading -- //

int8_t GameProcess::readInt8(void* addr)
{
	int8_t value{};
	ReadProcessMemory(hProcess, (LPCVOID)addr, (LPVOID)&value, 1, nullptr);
	return value;
}

int16_t GameProcess::readInt16(void* addr)
{
	int16_t value{};
	ReadProcessMemory(hProcess, (LPCVOID)addr, (LPVOID)&value, 2, nullptr);
	return value;
}

int32_t GameProcess::readInt32(void* addr)
{
	int16_t value{};
	ReadProcessMemory(hProcess, (LPCVOID)addr, (LPVOID)&value, 4, nullptr);
	return value;
}

int64_t GameProcess::readInt64(void* addr)
{
	int16_t value{};
	ReadProcessMemory(hProcess, (LPCVOID)addr, (LPVOID)&value, 8, nullptr);
	return value;
}

float GameProcess::readFloat(void* addr)
{
	float value{};
	ReadProcessMemory(hProcess, (LPCVOID)addr, (LPVOID)&value, 4, nullptr);
	return value;
}

void GameProcess::readBytes(void* addr, void* buf, size_t readSize)
{
	ReadProcessMemory(hProcess, (LPCVOID)addr, (LPVOID)&buf, readSize, nullptr);
}

// -- Public : Writing -- // 

void GameProcess::writeInt8(void* addr, int8_t value)
{
	WriteProcessMemory(hProcess, (LPVOID)addr, (LPCVOID)&value, 1, nullptr);
}

void  GameProcess::writeInt16(void* addr, int16_t value)
{
	WriteProcessMemory(hProcess, (LPVOID)addr, (LPCVOID)&value, 2, nullptr);
}

void  GameProcess::writeInt32(void* addr, int32_t value)
{
	WriteProcessMemory(hProcess, (LPVOID)addr, (LPCVOID)&value, 4, nullptr);
}

void  GameProcess::writeInt64(void* addr, int64_t value)
{
	WriteProcessMemory(hProcess, (LPVOID)addr, (LPCVOID)&value, 8, nullptr);
}

void  GameProcess::writeFloat(void* addr, float value)
{
	WriteProcessMemory(hProcess, (LPVOID)addr, (LPCVOID)&value, 4, nullptr);
}

void  GameProcess::writeBytes(void* addr, void* buf, size_t bufSize)
{
	WriteProcessMemory(hProcess, (LPVOID)addr, (LPCVOID)buf, bufSize, nullptr);
}

// -- Public : Allocation & Freeing -- // 

void* GameProcess::allocateMem(size_t amount)
{
	return VirtualAllocEx(hProcess, 0, amount, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
}

void GameProcess::freeMem(void* addr)
{
	VirtualFreeEx(hProcess, (LPVOID)addr, 0, MEM_RELEASE);
}