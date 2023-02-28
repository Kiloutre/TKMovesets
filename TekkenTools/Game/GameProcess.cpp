#include <windows.h>
#include <tlhelp32.h>

#include "GameProcess.hpp"

// -- Private : Helpers -- //

DWORD GameProcess::GetGamePID(const char* processName)
{
	HANDLE hProcessSnap;
	PROCESSENTRY32 pe32;
	hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

	if (GetLastError() != ERROR_ACCESS_DENIED)
	{
		pe32.dwSize = sizeof(PROCESSENTRY32);
		if (Process32First(hProcessSnap, &pe32)) {
			if (strcmp(pe32.szExeFile, processName) == 0) {
				DWORD pid = pe32.th32ProcessID;
				CloseHandle(hProcessSnap);
				return pid;
			}

			while (Process32Next(hProcessSnap, &pe32)) {
				if (strcmp(pe32.szExeFile, processName) == 0) {
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

bool GameProcess::LoadGameMainModule(const char* processName, DWORD pid)
{
	HANDLE moduleSnap;
	MODULEENTRY32 me32 = {};

	moduleSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);

	if (GetLastError() != ERROR_ACCESS_DENIED)
	{
		me32.dwSize = sizeof(MODULEENTRY32);
		if (Module32First(moduleSnap, &me32)) {
			if (strcmp(me32.szModule, processName) == 0) {
				modBaseAddr = (int64_t)me32.modBaseAddr;
				CloseHandle(moduleSnap);
				return true;
			}

			while (Module32Next(moduleSnap, &me32)) {
				if (strcmp(me32.szModule, processName) == 0) {

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

GameProcessError GameProcess::AttachToNamedProcess(const char* processName)
{
	DWORD pid = GetGamePID(processName);

	if (pid == (DWORD)-1) return PROC_NOT_FOUND;
	else {
		hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
		if (hProcess != nullptr && LoadGameMainModule(processName, pid)) {
			processId = pid;
			return PROC_ATTACHED;
		}
		else {
			return PROC_ATTACH_ERR;
		}
	}
}

// -- Public -- //

bool GameProcess::Attach(const char* processName)
{
	// Another thread might access errcode before AttachToNameProcess() may return, that is why we set it first
	errcode = PROC_ATTACHING;
	errcode = AttachToNamedProcess(processName);
	return errcode == PROC_ATTACHED;
}

void GameProcess::DetachFromGame()
{
	if (hProcess != nullptr) {
		CloseHandle(hProcess);
		hProcess = nullptr;
	}
	errcode = PROC_NOT_ATTACHED;
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