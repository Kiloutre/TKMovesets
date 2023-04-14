#include <windows.h>
#include <tlhelp32.h>

#include "GameProcess.hpp"
#include "Helpers.hpp"

#include "constants.h"
#include "GameAddresses.h"

// Utils //

namespace GameProcessUtils
{
	std::vector<processEntry> GetRunningProcessList()
	{
		HANDLE hProcessSnap;
		PROCESSENTRY32 pe32{ 0 };
		hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

		std::vector<processEntry> processList;

		if (GetLastError() != ERROR_ACCESS_DENIED)
		{
			pe32.dwSize = sizeof(PROCESSENTRY32);
			if (Process32First(hProcessSnap, &pe32)) {
				processList.push_back({
					.name = pe32.szExeFile,
					.pid = pe32.th32ProcessID
					});

				while (Process32Next(hProcessSnap, &pe32)) {
					processList.push_back({
						.name = pe32.szExeFile,
						.pid = pe32.th32ProcessID
						});
				}
			}

			CloseHandle(hProcessSnap);
		}
		return processList;
	};
};


// -- Private methods  -- //

GameProcessErrcode_ GameProcess::AttachToNamedProcess(const char* processName, DWORD processExtraFlags)
{
	DWORD pid = GetGamePID(processName);

	if (pid == (DWORD)-1) {
		return GameProcessErrcode_PROC_NOT_FOUND;
	}
	else {
		m_processHandle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | processExtraFlags, FALSE, pid);
		if (m_processHandle != nullptr)
		{
			if (LoadGameMainModule(processName))
			{
				processId = pid;
				return GameProcessErrcode_PROC_ATTACHED;
			}
			CloseHandle(m_processHandle);
		}
		return GameProcessErrcode_PROC_ATTACH_ERR;
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
				m_pid = pe32.th32ProcessID;
				CloseHandle(hProcessSnap);
				return m_pid;
			}

			while (Process32Next(hProcessSnap, &pe32)) {
				if (strcmp(pe32.szExeFile, processName) == 0) {
					m_pid = pe32.th32ProcessID;
					CloseHandle(hProcessSnap);
					return m_pid;
				}
			}
		}
		CloseHandle(hProcessSnap);
	}

	return (DWORD)-1;
}

std::vector<moduleEntry> GameProcess::GetModuleList()
{
	HANDLE moduleSnap;
	MODULEENTRY32 me32{ 0 };

	moduleAddr = -1;
	std::vector<moduleEntry> modules;

	moduleSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, m_pid);

	if (GetLastError() != ERROR_ACCESS_DENIED)
	{
		me32.dwSize = sizeof(MODULEENTRY32);
		if (Module32First(moduleSnap, &me32)) {
			modules.push_back({
				.name = std::string(me32.szModule),
				.address = (gameAddr)me32.modBaseAddr,
				.size = (uint64_t)me32.modBaseSize
			});

			while (Module32Next(moduleSnap, &me32)) {
				modules.push_back({
					.name = std::string(me32.szModule),
					.address = (gameAddr)me32.modBaseAddr,
					.size = (uint64_t)me32.modBaseSize
				});
			}
		}
	}

	CloseHandle(moduleSnap);
	return modules;
}

bool GameProcess::LoadGameMainModule(const char* processName)
{
	std::vector<moduleEntry> modules = GetModuleList();

	for (auto& module : modules)
	{
		if (module.name == processName)
		{
			moduleAddr = module.address;
			moduleSize = module.size;
		}
	}

	return moduleAddr != -1;
}

// -- Public methods -- //

bool GameProcess::Attach(const char* processName, DWORD processExtraFlags)
{
	if (status == GameProcessErrcode_PROC_ATTACHED) {
		return false;
	}

	allocatedMemory.clear();
	m_toFree.clear();
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
	if (m_processHandle != nullptr)
	{
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

void GameProcess::FreeOldGameMemory(bool instant)
{
	uint64_t currentDate = Helpers::getCurrentTimestamp();
	for (int i = 0; i < m_toFree.size();)
	{
		auto& [date, targetAddr] = m_toFree[i];
		if (instant || ((currentDate - date) >= GAME_FREEING_DELAY_SEC))
		{
			VirtualFreeEx(m_processHandle, (LPVOID)targetAddr, 0, MEM_RELEASE);
			m_toFree.erase(m_toFree.begin() + i, m_toFree.begin() + i + 1);
			DEBUG_LOG("Freeing game memory %llx after delay\n", targetAddr);
		}
		else {
			++i;
		}
	}
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

uint8_t GameProcess::readUInt8(gameAddr addr)
{
	uint8_t value{ (uint8_t)-1};
	ReadProcessMemory(m_processHandle, (LPCVOID)addr, (LPVOID)&value, 1, nullptr);
	return value;
}

uint16_t GameProcess::readUInt16(gameAddr addr)
{
	uint16_t value{ (uint16_t)-1};
	ReadProcessMemory(m_processHandle, (LPCVOID)addr, (LPVOID)&value, 2, nullptr);
	return value;
}

uint32_t GameProcess::readUInt32(gameAddr addr)
{
	uint32_t value{ (uint32_t)-1};
	ReadProcessMemory(m_processHandle, (LPCVOID)addr, (LPVOID)&value, 4, nullptr);
	return value;
}

uint64_t GameProcess::readUInt64(gameAddr addr)
{
	uint64_t value{ (uint64_t)- 1};
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

void GameProcess::writeUInt8(gameAddr addr, uint8_t value)
{
	WriteProcessMemory(m_processHandle, (LPVOID)addr, (LPCVOID)&value, 1, nullptr);
}

void  GameProcess::writeUInt16(gameAddr addr, uint16_t value)
{
	WriteProcessMemory(m_processHandle, (LPVOID)addr, (LPCVOID)&value, 2, nullptr);
}

void  GameProcess::writeUInt32(gameAddr addr, uint32_t value)
{
	WriteProcessMemory(m_processHandle, (LPVOID)addr, (LPCVOID)&value, 4, nullptr);
}

void  GameProcess::writeUInt64(gameAddr addr, uint64_t value)
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
	if (allocatedBlock != 0) {
		allocatedMemory.push_back(std::pair<gameAddr, uint64_t>(allocatedBlock, amount));
		DEBUG_LOG("Allocated to game memory from : %llx to %llx\n", allocatedBlock, allocatedBlock + amount);
	}
	else {
		DEBUG_LOG("Allocation failure: attempt to allocate %lld (%llx) bytes\n", amount, amount);
	}

	return allocatedBlock;
}

void GameProcess::freeMem(gameAddr targetAddr)
{
	for (std::pair<gameAddr, uint64_t>& block : allocatedMemory) {
		if (block.first == (gameAddr)targetAddr)
		{
			allocatedMemory.erase(std::find(allocatedMemory.begin(), allocatedMemory.end(), block));
			DEBUG_LOG("Queuing freeing of allocated block %llx (size is %lld)\n", block.first, block.second);
			uint64_t currentDate = Helpers::getCurrentTimestamp();
			m_toFree.push_back({ currentDate, targetAddr });
			return;
		}
	}

	DEBUG_LOG("Freeing of allocated block %llx (not size found)\n", targetAddr);
	VirtualFreeEx(m_processHandle, (LPVOID)targetAddr, 0, MEM_RELEASE);
}

void GameProcess::createRemoteThread(gameAddr startAddress, uint64_t argument)
{
	CreateRemoteThread(m_processHandle, nullptr, 0, (LPTHREAD_START_ROUTINE)startAddress, &argument, 0, nullptr);
}

bool GameProcess::InjectDll(const char* fullpath)
{
	// Allocate space for dll path in process memory
	int fullpathSize = strlen(fullpath) + 1;
	gameAddr bufferAddr = allocateMem(fullpathSize);

	if (bufferAddr == 0) {
		return false;
	}
	// Write path in process memory
	writeBytes(bufferAddr, (void*)fullpath, fullpathSize);

	// Get address of LoadLibraryW
	PTHREAD_START_ROUTINE threatStartRoutineAddress = (PTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandle(TEXT("Kernel32")), "LoadLibraryW");

	// Start thread in remote process
	CreateRemoteThread(m_processHandle, NULL, 0, threatStartRoutineAddress, (PVOID)bufferAddr, 0, NULL);
	return true;
}