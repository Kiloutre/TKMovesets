#include <windows.h>
#include <tlhelp32.h>

#include "GameProcess.hpp"
#include "Helpers.hpp"

#include "constants.h"

// Utils //

namespace GameProcessUtils
{
	std::vector<processEntry> GetRunningProcessList()
	{
		HANDLE hProcessSnap;
		PROCESSENTRY32W pe32{ 0 };
		hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

		std::vector<processEntry> processList;

		if (GetLastError() != ERROR_ACCESS_DENIED)
		{
			pe32.dwSize = sizeof(PROCESSENTRY32W);
			if (Process32FirstW(hProcessSnap, &pe32)) {
				processList.push_back({
					.name = Helpers::wstring_to_string(pe32.szExeFile),
					.pid = pe32.th32ProcessID
				});

				while (Process32NextW(hProcessSnap, &pe32)) {
					processList.push_back({
						.name = Helpers::wstring_to_string(pe32.szExeFile),
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
	PROCESSENTRY32W pe32{ 0 };
	hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

	std::wstring w_processName = Helpers::string_to_wstring(processName);

	if (GetLastError() != ERROR_ACCESS_DENIED)
	{
		pe32.dwSize = sizeof(PROCESSENTRY32W);
		if (Process32FirstW(hProcessSnap, &pe32)) {
			if (wcscmp(pe32.szExeFile, w_processName.c_str()) == 0) {
				m_pid = pe32.th32ProcessID;
				CloseHandle(hProcessSnap);
				return m_pid;
			}

			while (Process32NextW(hProcessSnap, &pe32)) {
				if (wcscmp(pe32.szExeFile, w_processName.c_str()) == 0) {
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

std::vector<moduleEntry> GameProcess::GetModuleList() const
{
	HANDLE moduleSnap;
	MODULEENTRY32W me32{ 0 };

	std::vector<moduleEntry> modules;

	moduleSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, m_pid);

	if (GetLastError() != ERROR_ACCESS_DENIED)
	{
		me32.dwSize = sizeof(me32);
		if (Module32FirstW(moduleSnap, &me32)) {
			modules.push_back({
				.address = (gameAddr)me32.modBaseAddr,
				.name = Helpers::wstring_to_string(me32.szModule),
				.path = std::wstring(me32.szExePath),
				.size = (uint64_t)me32.modBaseSize
			});

			while (Module32NextW(moduleSnap, &me32)) {
				modules.push_back({
					.address = (gameAddr)me32.modBaseAddr,
					.name = Helpers::wstring_to_string(me32.szModule),
					.path = std::wstring(me32.szExePath),
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

	for (auto& m : modules)
	{
		if (m.name == processName)
		{
			mainModule = m;
			break;
		}
	}

	return mainModule.address != -1;
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
	mainModule.address = -1;
	status = GameProcessErrcode_PROC_NOT_ATTACHED;
}

bool GameProcess::IsAttached() const
{
	return status == GameProcessErrcode_PROC_ATTACHED;
}

bool GameProcess::CheckRunning()
{
	if (m_processHandle != nullptr)
	{
		int32_t value = 0;
		if (ReadProcessMemory(m_processHandle, (LPCVOID)mainModule.address, (LPVOID)&value, 4, nullptr) == 0)
		{
			DEBUG_LOG("! CheckRunning() failed: Process not running anymore !\n");
			Detach();
			status = GameProcessErrcode_PROC_EXITED;
			return false;
		}
		return true;
	}
	return false;
}

void GameProcess::FreeOldGameMemory(bool instant)
{
	uint64_t currentDate = Helpers::getCurrentTimestamp();
	for (int i = 0; i < m_toFree.size();)
	{
		auto& [date, targetAddr] = m_toFree[i];
		if (instant || ((currentDate - date) >= GAME_FREEING_DELAY_SEC))
		{
#pragma warning(push)
#pragma warning(disable:6001)
			DEBUG_LOG("Freeing game memory %llx after delay\n", targetAddr);
			VirtualFreeEx(m_processHandle, (LPVOID)targetAddr, 0, MEM_RELEASE);
			m_toFree.erase(m_toFree.begin() + i, m_toFree.begin() + i + 1);
#pragma warning(pop)
		}
		else {
			++i;
		}
	}
}

int8_t GameProcess::readInt8(gameAddr addr) const
{
	int8_t value{ -1 };
	ReadProcessMemory(m_processHandle, (LPCVOID)addr, (LPVOID)&value, 1, nullptr);
	return value;
}

int16_t GameProcess::readInt16(gameAddr addr) const
{
	int16_t value{ -1 };
	ReadProcessMemory(m_processHandle, (LPCVOID)addr, (LPVOID)&value, 2, nullptr);
	return value;
}

int32_t GameProcess::readInt32(gameAddr addr) const
{
	int32_t value{ -1 };
	ReadProcessMemory(m_processHandle, (LPCVOID)addr, (LPVOID)&value, 4, nullptr);
	return value;
}

int64_t GameProcess::readInt64(gameAddr addr) const
{
	int64_t value{ -1 };
	ReadProcessMemory(m_processHandle, (LPCVOID)addr, (LPVOID)&value, 8, nullptr);
	return value;
}

uint8_t GameProcess::readUInt8(gameAddr addr) const
{
	uint8_t value{ (uint8_t)-1};
	ReadProcessMemory(m_processHandle, (LPCVOID)addr, (LPVOID)&value, 1, nullptr);
	return value;
}

uint16_t GameProcess::readUInt16(gameAddr addr) const
{
	uint16_t value{ (uint16_t)-1};
	ReadProcessMemory(m_processHandle, (LPCVOID)addr, (LPVOID)&value, 2, nullptr);
	return value;
}

uint32_t GameProcess::readUInt32(gameAddr addr) const
{
	uint32_t value{ (uint32_t)-1};
	ReadProcessMemory(m_processHandle, (LPCVOID)addr, (LPVOID)&value, 4, nullptr);
	return value;
}

uint64_t GameProcess::readUInt64(gameAddr addr) const
{
	uint64_t value{ (uint64_t)- 1};
	ReadProcessMemory(m_processHandle, (LPCVOID)addr, (LPVOID)&value, 8, nullptr);
	return value;
}

float GameProcess::readFloat(gameAddr addr) const
{
	float value{ -1 };
	ReadProcessMemory(m_processHandle, (LPCVOID)addr, (LPVOID)&value, 4, nullptr);
	return value;
}

void GameProcess::readBytes(gameAddr addr, void* buf, size_t readSize) const
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


gameAddr GameProcess::allocateMem(size_t amount, bool executable)
{
	auto allocFlag = executable ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE;
	gameAddr allocatedBlock = (gameAddr)VirtualAllocEx(m_processHandle, nullptr, amount, MEM_COMMIT | MEM_RESERVE, allocFlag);
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

GameProcessThreadCreation_ GameProcess::createRemoteThread(gameAddr startAddress, uint64_t argument, bool waitEnd, int32_t* exitCodeThread)
{
	auto hThread = CreateRemoteThread(m_processHandle, nullptr, 0, (LPTHREAD_START_ROUTINE)startAddress, (PVOID)argument, 0, nullptr);

	bool result = hThread != nullptr;
	DEBUG_LOG("CreateRemoteThread at %llx, arg is %llx ; success = %d\n", startAddress, argument, result);
	if (hThread != nullptr && waitEnd) {
		switch (WaitForSingleObject(hThread, 10000))
		{
		case WAIT_TIMEOUT:
			DEBUG_LOG("Thread: not waiting for more than 10secs for it to end.\n");
			return GameProcessThreadCreation_WaitTimeout;
		default:
			DEBUG_LOG("Thread finished.\n");
			if (exitCodeThread != nullptr) {
				GetExitCodeThread(hThread, (DWORD*)exitCodeThread);
			}
			return GameProcessThreadCreation_FinishedCleanly;
		}
	}
	return GameProcessThreadCreation_Error;
}

bool GameProcess::ReflectInjectDll(const Byte* orig_dllBytes, uint64_t dllSize)
{
	// -- THIS DOESN'T WORK AS OF YET -- //

	DEBUG_LOG("::ReflectInjectDll(, %llu)\n", dllSize);

	PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)orig_dllBytes;
	PIMAGE_NT_HEADERS ntHeaders = (PIMAGE_NT_HEADERS)(orig_dllBytes + dosHeader->e_lfanew);
	uint64_t sizeOfImage = ntHeaders->OptionalHeader.SizeOfImage;

	DEBUG_LOG("Alloc size %llu\n", sizeOfImage);
	Byte* dllCopy;
    try {
        dllCopy = new Byte[sizeOfImage];
    } catch (std::bad_alloc) {
        return false;
    }
    
	gameAddr dllGame = allocateMem(sizeOfImage, true);

	if (dllGame == 0) {
		DEBUG_LOG("Failed to allocate DLL within the target process.\n");
		return false;
	}

	DEBUG_LOG("Allocated at game %llx\n", dllGame);

	// Copy headers
	memcpy(dllCopy, orig_dllBytes, ntHeaders->OptionalHeader.SizeOfHeaders);

	// Copy each section
	PIMAGE_SECTION_HEADER sections = (PIMAGE_SECTION_HEADER)((uint64_t)ntHeaders + sizeof(IMAGE_NT_HEADERS));
	for (int i = 0; i < ntHeaders->FileHeader.NumberOfSections; ++i)
	{
		auto& section = sections[i];
		DEBUG_LOG("Copying section %d - %s at +0x%x\n", i, (char*)section.Name, section.VirtualAddress);
		void* target_section_addr = (void*)(dllCopy + section.VirtualAddress);
		void* source_section_addr = (void*)(orig_dllBytes + section.PointerToRawData);
		memcpy(target_section_addr, source_section_addr, section.SizeOfRawData);
	}

	// Rebuild import table
	auto& importDirectory = ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
	if (importDirectory.Size != 0)
	{
		PIMAGE_IMPORT_DESCRIPTOR importDescriptor = (PIMAGE_IMPORT_DESCRIPTOR)(dllCopy + importDirectory.VirtualAddress);

		while (importDescriptor->Name != NULL)
		{
			LPCSTR libName = (LPCSTR)(dllCopy + importDescriptor->Name);
			HMODULE libModule = LoadLibraryA(libName);
			DEBUG_LOG("%s = %p\n", libName, libModule);

			if (libModule == 0) {
				continue;
			}

			PIMAGE_THUNK_DATA nameRef = (PIMAGE_THUNK_DATA)(dllCopy + importDescriptor->Characteristics);
			PIMAGE_THUNK_DATA lpThunk = (PIMAGE_THUNK_DATA)(dllCopy + importDescriptor->FirstThunk);

			for (; nameRef->u1.AddressOfData != 0; nameRef++, lpThunk++)
			{
				if (nameRef->u1.AddressOfData & IMAGE_ORDINAL_FLAG) {
					*(FARPROC*)lpThunk = GetProcAddress(libModule, MAKEINTRESOURCEA(nameRef->u1.AddressOfData));
					DEBUG_LOG("Import: addr [%llx] = %p\n", nameRef->u1.AddressOfData, *(FARPROC*)lpThunk);
				}
				else {
					PIMAGE_IMPORT_BY_NAME thunkData = (PIMAGE_IMPORT_BY_NAME)(dllCopy + nameRef->u1.AddressOfData);
					*(FARPROC*)lpThunk = GetProcAddress(libModule, (LPCSTR)&thunkData->Name);
					DEBUG_LOG("Import: str [%s] = %p\n", thunkData->Name, *(FARPROC*)lpThunk);
				}
			}

			FreeLibrary(libModule);
			++importDescriptor;
		}
	}

	// Relocate
	uint64_t deltaImageBase = dllGame - ntHeaders->OptionalHeader.ImageBase;
	auto& relocDirectory = ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
	DEBUG_LOG("Delta is %lld (%llx)\n", deltaImageBase, deltaImageBase);

	typedef struct BASE_RELOCATION_BLOCK {
		DWORD PageAddress;
		DWORD BlockSize;
	} BASE_RELOCATION_BLOCK, * PBASE_RELOCATION_BLOCK;

	typedef struct BASE_RELOCATION_ENTRY {
		USHORT Offset : 12;
		USHORT Type : 4;
	} BASE_RELOCATION_ENTRY, * PBASE_RELOCATION_ENTRY;

	IMAGE_DATA_DIRECTORY relocations = ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
	DWORD_PTR relocationTable = relocations.VirtualAddress + (DWORD_PTR)dllCopy;
	DWORD relocationsProcessed = 0;

	while (relocationsProcessed < relocations.Size)
	{
		PBASE_RELOCATION_BLOCK relocationBlock = (PBASE_RELOCATION_BLOCK)(relocationTable + relocationsProcessed);
		relocationsProcessed += sizeof(BASE_RELOCATION_BLOCK);
		DWORD relocationsCount = (relocationBlock->BlockSize - sizeof(BASE_RELOCATION_BLOCK)) / sizeof(BASE_RELOCATION_ENTRY);
		PBASE_RELOCATION_ENTRY relocationEntries = (PBASE_RELOCATION_ENTRY)(relocationTable + relocationsProcessed);

		for (DWORD i = 0; i < relocationsCount; i++)
		{
			relocationsProcessed += sizeof(BASE_RELOCATION_ENTRY);

			if (relocationEntries[i].Type == 0)
			{
				continue;
			}

			DWORD_PTR relocationRVA = relocationBlock->PageAddress + relocationEntries[i].Offset;
			DWORD_PTR addressToPatch = 0;
			ReadProcessMemory(GetCurrentProcess(), (LPCVOID)((DWORD_PTR)dllCopy + relocationRVA), &addressToPatch, sizeof(DWORD_PTR), NULL);
			addressToPatch += deltaImageBase;
			std::memcpy((PVOID)((DWORD_PTR)dllCopy + relocationRVA), &addressToPatch, sizeof(DWORD_PTR));
		}
	}

	std::map<std::string, uint64_t> exportedFunctions;
	// DEBUG: Export table
	{
		auto& exportDir = ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
		PIMAGE_EXPORT_DIRECTORY exp = (PIMAGE_EXPORT_DIRECTORY)(dllCopy + exportDir.VirtualAddress);

		DEBUG_LOG("Export name - %s -\n", dllCopy + exp->Name);
		DEBUG_LOG("Name count: %u. Address count: %u.\n", exp->NumberOfNames, exp->NumberOfFunctions);

		DWORD* nameList = (DWORD*)(dllCopy + exp->AddressOfNames);
		DWORD* addrList = (DWORD*)(dllCopy + exp->AddressOfFunctions);
		for (unsigned int i = 0; i < exp->NumberOfNames; ++i)
		{
			char* nameAddr = (char*)(dllCopy + nameList[i]);
			if (strncmp(nameAddr, "MovesetLoader", sizeof("MovesetLoader") - 1) == 0)
			{
				exportedFunctions[nameAddr] = dllGame + addrList[i];
				DEBUG_LOG("EXPORTED FUNC [%s], OFFSET [%x], ABSOLUTE ADDR [%llx]\n", nameAddr, addrList[i], dllGame + addrList[i]);
			}
		}
	}

	DEBUG_LOG("Finished!\n");
	writeBytes(dllGame, dllCopy, sizeOfImage);

	LPTHREAD_START_ROUTINE entryPoint = (LPTHREAD_START_ROUTINE)(dllGame + ntHeaders->OptionalHeader.AddressOfEntryPoint);
	entryPoint = (LPTHREAD_START_ROUTINE)exportedFunctions["MovesetLoaderTest"];
	DEBUG_LOG("CreateRemoteThread at %p ...\n", entryPoint);
	//createRemoteThread((gameAddr)entryPoint, 0, true);

	delete[] dllCopy;
	// -- THIS DOESN'T WORK AS OF YET -- //
	return true;
}

bool GameProcess::InjectDll(const wchar_t* fullpath)
{
	{
		// Get .dll name only (including extension)
		std::string dllName;
		{
			std::wstring w_dllName = fullpath;
			if (w_dllName.find_last_of(L"/\\") != std::wstring::npos) {
				w_dllName.erase(0, w_dllName.find_last_of(L"/\\") + 1);
			}
			dllName = Helpers::wstring_to_string(w_dllName);
		}

		// Check if the DLL is not already injected. If so, don't bother injecting again.
		for (auto& module : GetModuleList())
		{
			if (module.name == dllName) {
				DEBUG_LOG("-- DLL '%s' already injected, not re-injecting. --\n", dllName.c_str());
				return true;
			}
		}
	}

	DEBUG_LOG("-- Injecting DLL %S --\n", fullpath);
	// Allocate space for dll path in process memory
	int fullpathSize = ((int)wcslen(fullpath) + 1) * (int)sizeof(WCHAR);
	gameAddr bufferAddr = allocateMem(fullpathSize);

	if (bufferAddr == 0) {
		DEBUG_LOG("DLL injection failure: tried to allocated %d, got nullptr from alloc.\n", fullpathSize);
		return false;
	}
	// Write path in process memory
	writeBytes(bufferAddr, (void*)fullpath, fullpathSize);

	// Get address of LoadLibraryW
	auto moduleHandle = GetModuleHandleA("Kernel32");
	if (moduleHandle == 0) {
		DEBUG_LOG("Failed getting the module handle of Kernel32.\n");
		return false;
	}
	PTHREAD_START_ROUTINE threatStartRoutineAddress = (PTHREAD_START_ROUTINE)GetProcAddress(moduleHandle, "LoadLibraryW");

	// Start thread in remote process
	auto errcode = createRemoteThread((gameAddr)threatStartRoutineAddress, bufferAddr, true);
	if (errcode != GameProcessThreadCreation_WaitTimeout) {
		// Only free if the thread is actually finished
		// Yes, this means a potential memory leak if the thread takes more than 10sec
		// Todo: if WaitTimeout, remember the thread handle and regularly check if it is still ongoing
		// Not really important here though
		freeMem(bufferAddr);
	}

	return errcode != GameProcessThreadCreation_Error;
}


gameAddr GameProcess::AOBScan(unsigned int startingBytes, const char* bytesString)
{
	// This has not been tested
	const unsigned int bufsize = 8192; // Must be a multiple of 4 at the very least
	const unsigned int readBufUpTo = bufsize - 4;
	unsigned char buffer[bufsize];

	unsigned int bytesString_len = Helpers::get_memory_string_length(bytesString);

	// Todo: limit to a single module. We know what we're looking for.
	for (auto& module : GetModuleList())
	{
		gameAddr startAddr = module.address;
		gameAddr endAddr = startAddr + (module.size / bytesString_len) * bytesString_len;
		memset(buffer, 0, bufsize);
		DEBUG_LOG("%s\n", module.name.c_str());
		while (startAddr < endAddr)
		{
			if (!ReadProcessMemory(m_processHandle, (LPCVOID)startAddr, (LPVOID)buffer, bufsize, nullptr))
			{
				DEBUG_LOG("ReadprocessMemory error\n");
				break;
			}

			for (unsigned int i = 0; i < readBufUpTo; ++i)
			{
				if (*(unsigned int*)&buffer[i] == startingBytes)
				{
					bool result;

					if (bufsize - i <= bytesString_len) {
						result = Helpers::compare_memory_string(&buffer[i], bytesString);
					} if (endAddr - startAddr <= bytesString_len) {
						unsigned char* tmp_buffer = new unsigned char[bytesString_len];
						if (!ReadProcessMemory(m_processHandle, (LPCVOID)startAddr, (LPVOID)tmp_buffer, bytesString_len, nullptr))
						{
							DEBUG_LOG("ReadprocessMemory error\n");
							startAddr = endAddr;
							break;
						}
						result = Helpers::compare_memory_string(tmp_buffer, bytesString);
						delete[] tmp_buffer;
					}
					else {
						result = false;
					}

					if (result) {
						return startAddr + i;
					}
				}	
			}
			startAddr += bufsize;
		}
	}
	DEBUG_LOG("fin\n");
	
	return 0;
}