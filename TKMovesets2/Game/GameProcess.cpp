#include <windows.h>
#include <tlhelp32.h>

#include "GameProcess.hpp"

#include "GameAddresses.h"

#pragma warning( disable : C6001 )

// -- Private methods  -- //

GameProcessErrcode_ GameProcess::AttachToNamedProcess(const char* processName, DWORD processExtraFlags)
{
	DWORD pid = GetGamePID(processName);

	if (pid == (DWORD)-1) return GameProcessErrcode_PROC_NOT_FOUND;
	else {
		m_processHandle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | processExtraFlags, FALSE, pid);
		if (m_processHandle != nullptr && LoadGameMainModule(processName, pid)) {
			processId = pid;
			return GameProcessErrcode_PROC_ATTACHED;
		}
		else {
			return GameProcessErrcode_PROC_ATTACH_ERR;
		}
	}
}

DWORD GameProcess::GetGamePID(const char* processName)
{
	HANDLE hProcessSnap;
	PROCESSENTRY32 pe32{ 0 };
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
	MODULEENTRY32 me32{ 0 };

	moduleAddr = -1;
	m_moduleInfos.clear();

	moduleSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);

	if (GetLastError() != ERROR_ACCESS_DENIED)
	{
		me32.dwSize = sizeof(MODULEENTRY32);
		if (Module32First(moduleSnap, &me32)) {

			m_moduleInfos.push_back(std::pair<gameAddr, uint64_t>((gameAddr)me32.modBaseAddr, (uint64_t)me32.modBaseSize));

			if (strcmp(me32.szModule, processName) == 0) {
				moduleAddr = (gameAddr)me32.modBaseAddr;
				moduleSize = (uint64_t)me32.modBaseSize;
			}

			while (Module32Next(moduleSnap, &me32)) {
				m_moduleInfos.push_back(std::pair<gameAddr, uint64_t>((gameAddr)me32.modBaseAddr, (uint64_t)me32.modBaseSize));

				if (strcmp(me32.szModule, processName) == 0) {

					moduleAddr = (gameAddr)me32.modBaseAddr;
					moduleSize = (uint64_t)me32.modBaseSize;
				}
			}
		}
	}

	// Sort them by address for ease of iteration
	/*
	std::sort(m_moduleInfos.begin(), m_moduleInfos.end(), [](auto& left, auto& right) {
		return left.first < right.first;
	});
	*/

	CloseHandle(moduleSnap);
	return moduleAddr != -1;
}

// -- Public methods -- //

bool GameProcess::Attach(const char* processName, DWORD processExtraFlags)
{
	if (status == GameProcessErrcode_PROC_ATTACHED) {
		return false;
	}

	allocatedMemory.clear();
	status = AttachToNamedProcess(processName, processExtraFlags);
	return status == GameProcessErrcode_PROC_ATTACHED;
}

void GameProcess::Detach()
{
	if (m_processHandle != nullptr) {
		CloseHandle(m_processHandle);
		m_processHandle = nullptr;
	}
	status = GameProcessErrcode_PROC_NOT_ATTACHED;
}

bool GameProcess::IsAttached()
{
	return status == GameProcessErrcode_PROC_ATTACHED;
}

bool GameProcess::CheckRunning()
{
	if (m_processHandle != nullptr) {
		int32_t value = 0;
		if (ReadProcessMemory(m_processHandle, (LPCVOID)moduleAddr, (LPVOID)&value, 4, nullptr) == 0)
		{
			Detach();
			status = GameProcessErrcode_PROC_EXITED;
			return false;
		}
	}
	return true;
}


int8_t GameProcess::readInt8(gameAddr addr)
{
	int8_t value{ -1 };
	ReadProcessMemory(m_processHandle, (LPCVOID)addr, (LPVOID)&value, 1, nullptr);
	return value;
}

int16_t GameProcess::readInt16(gameAddr addr)
{
	int16_t value{ -1 };
	ReadProcessMemory(m_processHandle, (LPCVOID)addr, (LPVOID)&value, 2, nullptr);
	return value;
}

int32_t GameProcess::readInt32(gameAddr addr)
{
	int32_t value{ -1 };
	ReadProcessMemory(m_processHandle, (LPCVOID)addr, (LPVOID)&value, 4, nullptr);
	return value;
}

int64_t GameProcess::readInt64(gameAddr addr)
{
	int64_t value{ -1 };
	ReadProcessMemory(m_processHandle, (LPCVOID)addr, (LPVOID)&value, 8, nullptr);
	return value;
}

float GameProcess::readFloat(gameAddr addr)
{
	float value{ -1 };
	ReadProcessMemory(m_processHandle, (LPCVOID)addr, (LPVOID)&value, 4, nullptr);
	return value;
}

void GameProcess::readBytes(gameAddr addr, void* buf, size_t readSize)
{
	ReadProcessMemory(m_processHandle, (LPCVOID)addr, (LPVOID)buf, readSize, nullptr);
}


void GameProcess::writeInt8(gameAddr addr, int8_t value)
{
	WriteProcessMemory(m_processHandle, (LPVOID)addr, (LPCVOID)&value, 1, nullptr);
}

void  GameProcess::writeInt16(gameAddr addr, int16_t value)
{
	WriteProcessMemory(m_processHandle, (LPVOID)addr, (LPCVOID)&value, 2, nullptr);
}

void  GameProcess::writeInt32(gameAddr addr, int32_t value)
{
	WriteProcessMemory(m_processHandle, (LPVOID)addr, (LPCVOID)&value, 4, nullptr);
}

void  GameProcess::writeInt64(gameAddr addr, int64_t value)
{
	WriteProcessMemory(m_processHandle, (LPVOID)addr, (LPCVOID)&value, 8, nullptr);
}

void  GameProcess::writeFloat(gameAddr addr, float value)
{
	WriteProcessMemory(m_processHandle, (LPVOID)addr, (LPCVOID)&value, 4, nullptr);
}

void  GameProcess::writeBytes(gameAddr addr, void* buf, size_t bufSize)
{
	WriteProcessMemory(m_processHandle, (LPVOID)addr, (LPCVOID)buf, bufSize, nullptr);
}


gameAddr GameProcess::allocateMem(size_t amount)
{
	gameAddr allocatedBlock = (gameAddr)VirtualAllocEx(m_processHandle, nullptr, amount, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	allocatedMemory.push_back(std::pair<gameAddr, uint64_t>(allocatedBlock, amount));
	printf("Allocated to game memory from %llx to %llx\n", allocatedBlock, allocatedBlock + amount);
	return allocatedBlock;
}

void GameProcess::freeMem(gameAddr addr)
{
	VirtualFreeEx(m_processHandle, (LPVOID)addr, 0, MEM_RELEASE);
	for (std::pair<gameAddr, uint64_t>& block : allocatedMemory) {
		if (block.first == addr) {
			printf("Freed allocated block %llx (size %lld)\n", block.first, block.second);
			allocatedMemory.erase(std::find(allocatedMemory.begin(), allocatedMemory.end(), block));
			return;
		}
	}
}