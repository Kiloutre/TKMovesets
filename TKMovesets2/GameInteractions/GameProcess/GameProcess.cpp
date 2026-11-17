#include <filesystem>
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

		if (hProcessSnap != INVALID_HANDLE_VALUE)
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


	struct t_enumWindowsParams
	{
		std::vector <std::string>& names;
		DWORD pid;

	};

	static void OnWindowEnum(HWND hwnd, t_enumWindowsParams& param)
	{
		DWORD pid;
		GetWindowThreadProcessId(hwnd, &pid);
		if (pid != param.pid) {
			return;
		}

		char windowName[256];
		if (GetWindowTextA(hwnd, windowName, sizeof(windowName)) == 0) {
			return;
		}

		DEBUG_LOG("%s\n", windowName);
		param.names.push_back(windowName);
	}

	std::vector<std::string> GetWindowsList(DWORD pid)
	{
		std::vector <std::string> names;

		t_enumWindowsParams params = { .names = names, .pid = pid };
		DEBUG_LOG("-- EnumWindows --\n");
		EnumWindows((WNDENUMPROC)OnWindowEnum, (LPARAM)&params);
		DEBUG_LOG("-- EnumWindows END --\n");
		return names;
	}

	bool FindWindowText(DWORD pid, const char* substring)
	{
		auto windowList = GetWindowsList(pid);

		for (const std::string& wName : windowList)
		{
			if (wName.find(substring) != std::string::npos) {
				DEBUG_LOG("Matched window name '%s'\n", wName.c_str());
				return true;
			}
		}
		return false;
	}

	bool FindWindowText(DWORD pid, const std::vector<const char*>& substrings)
	{
		auto windowList = GetWindowsList(pid);

		for (const std::string& wName : windowList)
		{
			for (const auto& substring : substrings)
			{
				if (wName.find(substring) != std::string::npos) {
					DEBUG_LOG("Matched window name '%s'\n", wName.c_str());
					return true;
				}
			}
		}
		return false;
	}
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
			m_processHandle = nullptr;
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

	if (hProcessSnap != INVALID_HANDLE_VALUE)
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

gameAddr GameProcess::ResolveRemoteFunction(FARPROC function) const
{
	if (!function) return 0;
	MEMORY_BASIC_INFORMATION region{};
	if (!VirtualQuery(reinterpret_cast<const void*>(function), &region, sizeof(region))) return 0;
	wchar_t path[MAX_PATH];
	auto length = GetModuleFileNameW(static_cast<HMODULE>(region.AllocationBase), path, MAX_PATH);
	if (!length || length == MAX_PATH) return 0;
	auto moduleName = std::filesystem::path(path).filename().wstring();
	const auto offset = reinterpret_cast<gameAddr>(function) - reinterpret_cast<gameAddr>(region.AllocationBase);
	for (const auto& module : GetModuleList()) {
		if (_wcsicmp(std::filesystem::path(module.path).filename().c_str(), moduleName.c_str()) == 0 && offset < module.size)
			return module.address + offset;
	}
	return 0;
}

gameAddr GameProcess::RemoteExport(const char* moduleName, const char* functionName) const
{
	for (const auto& module : GetModuleList()) {
		if (_stricmp(module.name.c_str(), moduleName) != 0) continue;
		const gameAddr base = module.address;
		auto inModule = [&](uint64_t rva, uint64_t size) { return rva < module.size && size <= module.size - rva; };
		IMAGE_DOS_HEADER dos{};
		if (!readBytes(base, &dos, sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE || !inModule((uint32_t)dos.e_lfanew, sizeof(IMAGE_NT_HEADERS64))) return 0;
		IMAGE_NT_HEADERS64 nt{};
		if (!readBytes(base + (uint32_t)dos.e_lfanew, &nt, sizeof(nt)) || nt.Signature != IMAGE_NT_SIGNATURE ||
			nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC || nt.OptionalHeader.NumberOfRvaAndSizes <= IMAGE_DIRECTORY_ENTRY_EXPORT) return 0;
		const auto& directory = nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
		IMAGE_EXPORT_DIRECTORY exports{};
		if (!directory.VirtualAddress || !inModule(directory.VirtualAddress, sizeof(exports)) ||
			!readBytes(base + directory.VirtualAddress, &exports, sizeof(exports))) return 0;
		if (!inModule(exports.AddressOfNames, uint64_t(exports.NumberOfNames) * 4) || !inModule(exports.AddressOfNameOrdinals, uint64_t(exports.NumberOfNames) * 2) ||
			!inModule(exports.AddressOfFunctions, uint64_t(exports.NumberOfFunctions) * 4)) return 0;
		const size_t wanted = strlen(functionName);
		std::vector<char> name(wanted + 1);
		for (DWORD i = 0; i < exports.NumberOfNames; ++i) {
			uint32_t nameRva = 0;
			if (!readBytes(base + exports.AddressOfNames + uint64_t(i) * 4, &nameRva, 4) || !inModule(nameRva, name.size())) continue;
			if (!readBytes(base + nameRva, name.data(), name.size()) || memcmp(name.data(), functionName, wanted + 1) != 0) continue;
			uint16_t ordinal = 0;
			uint32_t functionRva = 0;
			if (!readBytes(base + exports.AddressOfNameOrdinals + uint64_t(i) * 2, &ordinal, 2) || ordinal >= exports.NumberOfFunctions ||
				!readBytes(base + exports.AddressOfFunctions + uint64_t(ordinal) * 4, &functionRva, 4) || !inModule(functionRva, 1)) return 0;
			// A forwarded export points inside the export directory: not code
			if (functionRva >= directory.VirtualAddress && functionRva < uint64_t(directory.VirtualAddress) + directory.Size) return 0;
			return base + functionRva;
		}
		return 0;
	}
	return 0;
}

std::vector<moduleEntry> GameProcess::GetModuleList() const
{
	HANDLE moduleSnap;
	MODULEENTRY32W me32{ 0 };

	std::vector<moduleEntry> modules;

	moduleSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, m_pid);

	if (moduleSnap == INVALID_HANDLE_VALUE) return modules;
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
			return true;
		}
	}

	return false;
}

// -- Public methods -- //

bool GameProcess::Attach(const char* processName, DWORD processExtraFlags)
{
	std::lock_guard lock(m_resourceMutex);
	if (status == GameProcessErrcode_PROC_ATTACHED) {
		return false;
	}

    Detach();
	status = AttachToNamedProcess(processName, processExtraFlags);
	return status == GameProcessErrcode_PROC_ATTACHED;
}

bool GameProcess::AttachById(DWORD pid, DWORD flags)
{
    std::lock_guard lock(m_resourceMutex);
    if (IsAttached()) return false;
    Detach();
    m_pid = pid;
    m_processHandle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | flags, FALSE, pid);
    if (!m_processHandle) { status = GameProcessErrcode_PROC_ATTACH_ERR; return false; }
    const auto modules = GetModuleList();
    if (modules.empty()) { Detach(); status = GameProcessErrcode_PROC_ATTACH_ERR; return false; }
    mainModule = modules.front(); processId = pid;
    status = GameProcessErrcode_PROC_ATTACHED;
    return true;
}

void GameProcess::Detach()
{
    std::lock_guard lock(m_resourceMutex);
    if (m_suspensionHandle) {
        // Never leave a live game suspended; a handle must never survive into another attachment either
        if (!ResumeProcessRaw()) ResumeProcessRaw();
        if (m_suspensionHandle) {
            DEBUG_LOG("Detach(): could not resume the game process\n");
            CloseHandle(m_suspensionHandle);
            m_suspensionHandle = nullptr;
        }
    }
    ++m_generation;
    processId = static_cast<DWORD>(-1); m_pid = 0;
    for (auto& thread : m_pendingThreads) CloseHandle(thread.handle);
    m_pendingThreads.clear();
    allocatedMemory.clear(); m_toFree.clear(); m_pinnedAllocations.clear();
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
	std::lock_guard lock(m_resourceMutex);
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
    std::lock_guard lock(m_resourceMutex);
    ReapRemoteThreads();
	uint64_t currentDate = Helpers::getCurrentTimestamp();
	for (int i = 0; i < m_toFree.size();)
	{
		auto& [date, targetAddr] = m_toFree[i];
		if (instant || ((currentDate - date) >= GAME_FREEING_DELAY_SEC))
		{
#pragma warning(push)
#pragma warning(disable:6001)
			DEBUG_LOG("Freeing game memory %llx after delay\n", targetAddr);
			ReleaseMemoryRaw(targetAddr);
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
	readBytes(addr, &value, sizeof(value));
	return value;
}

int16_t GameProcess::readInt16(gameAddr addr) const
{
	int16_t value{ -1 };
	readBytes(addr, &value, sizeof(value));
	return value;
}

int32_t GameProcess::readInt32(gameAddr addr) const
{
	int32_t value{ -1 };
	readBytes(addr, &value, sizeof(value));
	return value;
}

int64_t GameProcess::readInt64(gameAddr addr) const
{
	int64_t value{ -1 };
	readBytes(addr, &value, sizeof(value));
	return value;
}

uint8_t GameProcess::readUInt8(gameAddr addr) const
{
	uint8_t value{ (uint8_t)-1};
	readBytes(addr, &value, sizeof(value));
	return value;
}

uint16_t GameProcess::readUInt16(gameAddr addr) const
{
	uint16_t value{ (uint16_t)-1};
	readBytes(addr, &value, sizeof(value));
	return value;
}

uint32_t GameProcess::readUInt32(gameAddr addr) const
{
	uint32_t value{ (uint32_t)-1};
	readBytes(addr, &value, sizeof(value));
	return value;
}

uint64_t GameProcess::readUInt64(gameAddr addr) const
{
	uint64_t value{ (uint64_t)- 1};
	readBytes(addr, &value, sizeof(value));
	return value;
}

float GameProcess::readFloat(gameAddr addr) const
{
	float value{ -1 };
	readBytes(addr, &value, sizeof(value));
	return value;
}

bool GameProcess::readBytes(gameAddr address, void* data, size_t size) const
{
    const bool ok = probeBytes(address, data, size);
    if (!ok) { if (auto batch = ActiveBatch()) batch->m_failed = true; return false; }
    return true;
}
bool GameProcess::probeBytes(gameAddr address, void* data, size_t size) const
{
    std::lock_guard lock(m_resourceMutex);
    auto batch = ActiveBatch();
    if (batch && batch->m_generation != m_generation) { SetLastError(ERROR_INVALID_HANDLE); return false; }
    if (!ReadMemoryRaw(address, data, size)) return false;
    return !batch || batch->Overlay(address, data, size);
}
GameProcess::WriteBatch* GameProcess::ActiveBatch() const
{
    std::lock_guard lock(m_resourceMutex);
    if (m_activeWriteBatch != nullptr && m_activeWriteBatch->m_owner == std::this_thread::get_id()) {
        return m_activeWriteBatch;
    }
    return nullptr;
}


bool GameProcess::writeInt8(gameAddr addr, int8_t value)
{
	return writeBytes(addr, &value, sizeof(value));
}

bool GameProcess::writeInt16(gameAddr addr, int16_t value)
{
	return writeBytes(addr, &value, sizeof(value));
}

bool GameProcess::writeInt32(gameAddr addr, int32_t value)
{
	return writeBytes(addr, &value, sizeof(value));
}

bool GameProcess::writeInt64(gameAddr addr, int64_t value)
{
	return writeBytes(addr, &value, sizeof(value));
}

bool GameProcess::writeUInt8(gameAddr addr, uint8_t value)
{
	return writeBytes(addr, &value, sizeof(value));
}

bool GameProcess::writeUInt16(gameAddr addr, uint16_t value)
{
	return writeBytes(addr, &value, sizeof(value));
}

bool GameProcess::writeUInt32(gameAddr addr, uint32_t value)
{
	return writeBytes(addr, &value, sizeof(value));
}

bool GameProcess::writeUInt64(gameAddr addr, uint64_t value)
{
	return writeBytes(addr, &value, sizeof(value));
}

bool GameProcess::writeFloat(gameAddr addr, float value)
{
	return writeBytes(addr, &value, sizeof(value));
}

bool GameProcess::writeBytes(gameAddr address, const void* data, size_t size)
{
    std::lock_guard lock(m_resourceMutex);
    if (auto batch = ActiveBatch()) return batch->Stage(address, data, size);
    return WriteMemoryRaw(address, data, size);
}


gameAddr GameProcess::allocateMem(size_t size, bool executable)
{
    std::lock_guard lock(m_resourceMutex);
    auto batch = ActiveBatch();
    if (batch && batch->m_generation != m_generation) {
        batch->m_failed = true; SetLastError(ERROR_INVALID_HANDLE); return 0;
    }
    allocatedMemory.reserve(allocatedMemory.size() + 1);
    if (batch) batch->m_newAllocations.reserve(batch->m_newAllocations.size() + 1);
    auto address = AllocateMemoryRaw(size, executable);
    if (address) {
        allocatedMemory.emplace_back(address, size);
        if (batch) batch->m_newAllocations.emplace_back(address, size);
    } else if (batch) batch->m_failed = true;
    return address;
}

void GameProcess::freeMem(gameAddr targetAddr)
{
    std::lock_guard lock(m_resourceMutex);
    if (std::find(m_pinnedAllocations.begin(), m_pinnedAllocations.end(), targetAddr) != m_pinnedAllocations.end()) return;
    if (auto batch = ActiveBatch(); batch && batch->IsPrivate(targetAddr, 1)) {
        ReleaseNow(targetAddr);
        return;
    }
    for (auto& thread : m_pendingThreads) {
        auto block = std::find_if(allocatedMemory.begin(), allocatedMemory.end(), [&](const auto& entry) {
            return entry.first == targetAddr && thread.argument >= entry.first && thread.argument - entry.first < entry.second;
        });
        if (thread.argument == targetAddr || block != allocatedMemory.end()) { thread.releaseAddress = targetAddr; return; }
    }

	for (const auto block : allocatedMemory) {
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
	ReleaseMemoryRaw(targetAddr);
}

GameProcessThreadCreation_ GameProcess::createRemoteThread(gameAddr start, uint64_t argument, bool waitEnd, int32_t* exitCode, DWORD timeoutMs)
{
    HANDLE thread;
    uint64_t generation;
    {
        std::lock_guard lock(m_resourceMutex);
        if (auto batch = ActiveBatch()) {
            batch->m_failed = true;
            return GameProcessThreadCreation_Error;
        }
        ReapRemoteThreads();
        m_pendingThreads.reserve(m_pendingThreads.size() + 1);
        generation = m_generation;
        thread = CreateRemoteThread(m_processHandle, nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(start),
            reinterpret_cast<void*>(argument), 0, nullptr);
    }
    if (!thread) return GameProcessThreadCreation_Error;
    // Wait without holding the resource lock: the worker keeps reaping and freeing meanwhile
    auto result = GameProcessThreadCreation_Started;
    bool finished = false;
    if (waitEnd) {
        switch (WaitForSingleObject(thread, timeoutMs)) {
        case WAIT_OBJECT_0:
            finished = true;
            result = !exitCode || GetExitCodeThread(thread, reinterpret_cast<DWORD*>(exitCode))
                ? GameProcessThreadCreation_FinishedCleanly : GameProcessThreadCreation_Error;
            break;
        case WAIT_TIMEOUT: result = GameProcessThreadCreation_WaitTimeout; break;
        default: result = GameProcessThreadCreation_Error;
        }
    }
    std::lock_guard lock(m_resourceMutex);
    // A detach while waiting dropped the pending list of that attachment: do not carry this thread into the next one
    if (finished || generation != m_generation) CloseHandle(thread);
    else m_pendingThreads.push_back({thread, argument});
    return result;
}

bool GameProcess::ReflectInjectDll2(const Byte* orig_dllBytes, uint64_t dllSize)
{
	// -- THIS DOESN'T WORK AS OF YET -- //

	CONTEXT* t;

	DEBUG_LOG("::ReflectInjectDll(, %llu)\n", dllSize);

	PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)orig_dllBytes;
	PIMAGE_NT_HEADERS32 ntHeaders = (PIMAGE_NT_HEADERS32)(orig_dllBytes + dosHeader->e_lfanew);
	uint64_t sizeOfImage = ntHeaders->OptionalHeader.SizeOfImage;

	DEBUG_LOG("Alloc size %llu\n", sizeOfImage);
	Byte* dllCopy;
	try {
		dllCopy = new Byte[sizeOfImage];
	}
	catch (std::bad_alloc) {
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

bool GameProcess::ReflectInjectDll(const Byte* orig_dllBytes, uint64_t dllSize)
{
	// -- THIS DOESN'T WORK AS OF YET -- //

	CONTEXT* t;

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
	gameAddr bufferAddr;
	{
		// Not recorded in allocatedMemory: this may run on the injection thread while the worker iterates that list.
		// freeMem() releases untracked blocks directly, or once the pending remote thread using it has completed.
		std::lock_guard lock(m_resourceMutex);
		bufferAddr = AllocateMemoryRaw(fullpathSize, false);
	}

	if (bufferAddr == 0) {
		DEBUG_LOG("DLL injection failure: tried to allocated %d, got nullptr from alloc.\n", fullpathSize);
		return false;
	}
	// Write path in process memory
	if (!writeBytes(bufferAddr, fullpath, fullpathSize)) {
		freeMem(bufferAddr);
		return false;
	}

	// Get address of LoadLibraryW
	auto moduleHandle = GetModuleHandleA("Kernel32");
	if (moduleHandle == 0) {
		DEBUG_LOG("Failed getting the module handle of Kernel32.\n");
		freeMem(bufferAddr);
		return false;
	}
	auto loadLibrary = ResolveRemoteFunction(GetProcAddress(moduleHandle, "LoadLibraryW"));
	if (!loadLibrary) {
		freeMem(bufferAddr);
		return false;
	}

	// Start thread in remote process
	int32_t loadedModule = 0;
	auto errcode = createRemoteThread(loadLibrary, bufferAddr, true, &loadedModule);
    // freeMem retains the argument until a pending thread actually completes.
    freeMem(bufferAddr);

	return errcode == GameProcessThreadCreation_FinishedCleanly && loadedModule != 0;
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

GameProcess::~GameProcess()
{
	Detach();
}
