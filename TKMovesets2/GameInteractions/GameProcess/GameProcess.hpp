#pragma once

#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <utility>
#include <windows.h>

#include "GameTypes.h"

struct processEntry
{
	std::string name;
	DWORD pid;
};

#ifndef MODULE_ENTRY_DEF
#define MODULE_ENTRY_DEF
struct moduleEntry
{
	uint64_t address;
	std::string name;
	std::wstring path;
	uint64_t size;
};
#endif

namespace GameProcessUtils
{
	std::vector<processEntry> GetRunningProcessList();

	// Returns true if the process contains a window containing the substring
	bool FindWindowText(DWORD pid, const char* substring);
	bool FindWindowText(DWORD pid, const std::vector<const char*>& substrings);
};

// Thread creation status
enum GameProcessThreadCreation_
{
	GameProcessThreadCreation_FinishedCleanly,
	GameProcessThreadCreation_Started,
	GameProcessThreadCreation_WaitTimeout,
	GameProcessThreadCreation_Error,
};

enum GameProcessErrcode_
{
	// This will happen if the process-attaching thread is not started
	GameProcessErrcode_PROC_NOT_ATTACHED,
	// Process not found in the process list
	GameProcessErrcode_PROC_NOT_FOUND,
	// Currently searching for the game process or attempting to attach to it
	GameProcessErrcode_PROC_ATTACHING,
	// Found but not attached 
	GameProcessErrcode_PROC_ATTACH_ERR,
	// Found, attached, version mismatch
	GameProcessErrcode_PROC_VERSION_MISMATCH,
	// Attached with no error
	GameProcessErrcode_PROC_ATTACHED,
	// Unexpected exit
	GameProcessErrcode_PROC_EXITED,
};

class GameProcess
{
public:
    class WriteBatch;
    struct WriteResult { bool ok = false, rollbackComplete = true; DWORD error = ERROR_SUCCESS; };
private:
    // Guards the process/suspension handles, the allocation/free/pending-thread containers and m_activeWriteBatch.
    // The DLL injection thread uses this object concurrently with the worker thread. Leaf lock: never wait on
    // anything (remote threads, other locks) while holding it.
    mutable std::recursive_mutex m_resourceMutex;
    // Only applies to the thread that created it (see ActiveBatch())
    WriteBatch* m_activeWriteBatch = nullptr;
    std::atomic<uint64_t> m_generation = 0;
    std::vector<gameAddr> m_pinnedAllocations;
    struct PendingThread { HANDLE handle; gameAddr argument; gameAddr releaseAddress = 0; };
    std::vector<PendingThread> m_pendingThreads;
    void ReapRemoteThreads();
    bool ReleaseNow(gameAddr address);
    // Returns the write batch planned by the calling thread, nullptr if there is none (other threads write directly)
    WriteBatch* ActiveBatch() const;
protected:
    virtual bool ReadMemoryRaw(gameAddr address, void* data, size_t size) const;
    virtual bool WriteMemoryRaw(gameAddr address, const void* data, size_t size);
    virtual gameAddr AllocateMemoryRaw(size_t size, bool executable);
    virtual bool ReleaseMemoryRaw(gameAddr address);
    virtual bool SuspendProcessRaw();
    virtual bool ResumeProcessRaw();
private:
	// Stores the handle of the opened process
	HANDLE m_processHandle = nullptr;
    HANDLE m_suspensionHandle = nullptr;
	// Contains a list of <timestamp, gameAddr>, freeing gameAddr once timestamp is older than 10 seconds
	std::vector<std::pair<uint64_t, gameAddr>> m_toFree;
	// Stores the game pid
	DWORD m_pid = 0;

	// Attach to .processName
	GameProcessErrcode_ AttachToNamedProcess(const char* processName, DWORD processExtraFlags);
	// Returns the game PID
	DWORD GetGamePID(const char* processName);
	// Load informations about the main module in .mainModule
	bool LoadGameMainModule(const char* processName);
public:
    GameProcess() = default;
    virtual bool CanPrepareGameResources() const { return IsAttached() && processId != static_cast<DWORD>(-1); }
    GameProcess(const GameProcess&) = delete;
    GameProcess& operator=(const GameProcess&) = delete;
	virtual ~GameProcess();

	// Status of the process attachment
	GameProcessErrcode_ status{ GameProcessErrcode_PROC_NOT_ATTACHED };
	// Determines if a thread with Update() is currently running
	bool threadStarted{ false };
	// pid of process we latch on
	DWORD processId{ (DWORD)-1 };
	// Stores informations about the main module (address, size, name, exe path)
	moduleEntry mainModule{};
	// List of allocated blocks within the game's memory
	std::vector<std::pair<gameAddr, uint64_t>> allocatedMemory;
	// Base address. Every read & write will be done on base + addr.
	//gameAddr baseAddress{ (gameAddr)0x0 };

	// -- Interaction stuff -- //

    // Writes are staged. Fresh uploads precede shared pointer installation.
    // A failed installation restores originals; incomplete rollback pins new allocations.
    class WriteBatch {
        GameProcess& m_process;
        uint64_t m_generation;
        // Thread planning this batch: other threads using the same process are not staged into it
        std::thread::id m_owner = std::this_thread::get_id();
        struct Operation { gameAddr address; std::vector<Byte> bytes, before; bool privateUpload = false; };
        std::vector<Operation> m_operations;
        std::vector<Operation> m_expectations;
        std::vector<std::pair<gameAddr, uint64_t>> m_newAllocations;
        bool m_failed = false, m_committed = false, m_finished = false;
        WriteResult m_result;
        friend class GameProcess;
        bool Stage(gameAddr address, const void* bytes, size_t size);
        bool Overlay(gameAddr address, void* bytes, size_t size) const;
        bool IsPrivate(gameAddr address, size_t size) const;
    public:
        explicit WriteBatch(GameProcess& process);
        ~WriteBatch();
        WriteResult Commit();
        WriteBatch(const WriteBatch&) = delete;
        WriteBatch& operator=(const WriteBatch&) = delete;
    };

    class PauseGuard {
        GameProcess& m_process;
    public:
        explicit PauseGuard(GameProcess& process);
        ~PauseGuard();
        PauseGuard(const PauseGuard&) = delete;
        PauseGuard& operator=(const PauseGuard&) = delete;
    };
    // Commit-time preconditions prevent publishing into a changed round/player.
    bool ExpectBytes(gameAddr address, const void* bytes, size_t size);

	// Attach to a process, return false is failed. See .status for more details.
	bool Attach(const char* processName, DWORD processExtraFlags);
    bool AttachById(DWORD pid, DWORD processExtraFlags = 0);
	// Detach from the game,
	void Detach();
	// Returns true if .status = PROC_NOT_ATTACHED. Use CheckRunning() for an actual proper check.
	bool IsAttached() const;
	// Checks if pid still used by process & attempts a read. Updates .status & returns false if it fails.
	bool CheckRunning();
	// Frees previous allocated memory. Set [instant] to false (default) if you want to only free memory older than 10 seconds.
	void FreeOldGameMemory(bool instant = false);
	// Returns a list of module currently loaded in the target remote process
	virtual std::vector<moduleEntry> GetModuleList() const;
	gameAddr ResolveRemoteFunction(FARPROC function) const;
	// Address of [functionName] exported by the module named [moduleName] as loaded in the remote process, read from that
	// module's own export table (not from a local copy of the file). 0 if the module or the export is not there.
	gameAddr RemoteExport(const char* moduleName, const char* functionName) const;

	// Returns a list of window names for the windows attached to the process
	std::vector<std::string> GetWindowsList() const;


	// Reads an unsigned byte from the game in little endian
	int8_t  readInt8(gameAddr addr) const;
	// Reads a short (2B) from the game in little endian
	int16_t readInt16(gameAddr addr) const;
	// Reads an int (4B) from the game in little endian
	int32_t readInt32(gameAddr addr) const;
	// Reads an int (8B) from the game in little endian
	int64_t readInt64(gameAddr addr) const;

	// Reads a byte from the game in little endian
	uint8_t  readUInt8(gameAddr addr) const;
	// Reads a ushort (2B) from the game in little endian
	uint16_t readUInt16(gameAddr addr) const;
	// Reads an uint (4B) from the game in little endian
	uint32_t readUInt32(gameAddr addr) const;
	// Reads an uint (8B) from the game in little endian
	uint64_t readUInt64(gameAddr addr) const;

	// Reads a floating point number (4b) from the game in little endian
	float   readFloat(gameAddr addr) const;
	// Reads [readSize] amounts of bytes from the game and write them to the provided buffer
	bool    readBytes(gameAddr addr, void* buf, size_t readSize) const;
    // Optional dependency discovery; failure is handled by the caller.
    bool probeBytes(gameAddr addr, void* buf, size_t readSize) const;

	// Writes a value to the remote process's memory, adapting to the value type's size
	template<typename T>
    bool write(gameAddr addr, T value) { return writeBytes(addr, &value, sizeof(T)); }
	// Reads a value in the remote process's memory, adapting to the expected return value type's size
	template<typename T>
	T read(gameAddr addr) const
	{
		T value{ (T)-1 };
		readBytes(addr, &value, sizeof(T));
		return value;
	}

	// Writes a byte to the game
	bool writeInt8(gameAddr addr, int8_t value);
	// Writes a short (2B) to the game
	bool writeInt16(gameAddr addr, int16_t value);
	// Writes an int (4B) to the game
	bool writeInt32(gameAddr addr, int32_t value);
	// Writes an int (8B) to the game
	bool writeInt64(gameAddr addr, int64_t value);
	// Writes a byte to the game

	bool writeUInt8(gameAddr addr, uint8_t value);
	// Writes a short (2B) to the game
	bool writeUInt16(gameAddr addr, uint16_t value);
	// Writes an int (4B) to the game
	bool writeUInt32(gameAddr addr, uint32_t value);
	// Writes an int (8B) to the game
	bool writeUInt64(gameAddr addr, uint64_t value);

	// Writes a floating point number (4b) to the game
	bool writeFloat(gameAddr addr, float value);
	// Writes [bufSize] amounts of bytes to the game
	bool writeBytes(gameAddr addr, const void* buf, size_t bufSize);

	// Injects a DLL (from a file) inside of the process 
	bool InjectDll(const wchar_t* fullpath);
	// (DOES NOT WORK) Injects a DLL (from memory) inside of the process
	bool ReflectInjectDll(const Byte* dllBytes, uint64_t dllSize);
	bool ReflectInjectDll2(const Byte* dllBytes, uint64_t dllSize);

	// Allocates a certain amount of memory in the game
	gameAddr allocateMem(size_t amount, bool executable=false);
	// Frees memory that we previously allocated. Address must be the exact same as when it was returned by allocateMem().
	void freeMem(gameAddr addr);
	// Create a thread in the remote process. Can wait for the end of the thread and receive the thread exit code
	GameProcessThreadCreation_ createRemoteThread(gameAddr startAddress, uint64_t argument=0, bool waitEnd=false, int32_t* exitCodeThread=nullptr, DWORD timeoutMs=10000);
    size_t PendingRemoteThreadCount() const { std::lock_guard lock(m_resourceMutex); return m_pendingThreads.size(); }

	// Scans the main module for an AOB
	gameAddr AOBScan(unsigned int startingBytes, const char* bytesString);
};
