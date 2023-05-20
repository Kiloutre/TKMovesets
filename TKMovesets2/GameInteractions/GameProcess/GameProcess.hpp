#pragma once

#include <string>
#include <vector>
#include <thread>
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
};

// Thread creation status
enum GameProcessThreadCreation_
{
	GameProcessThreadCreation_FinishedCleanly,
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
private:
	// Stores the handle of the opened process
	HANDLE m_processHandle = nullptr;
	// Contains a list of <timestamp, gameAddr>, freeing gameAddr once timestamp is older than 10 seconds
	std::vector<std::pair<uint64_t, gameAddr>> m_toFree;
	// Stores the game pid
	DWORD m_pid;

	// Attach to .processName
	GameProcessErrcode_ AttachToNamedProcess(const char* processName, DWORD processExtraFlags);
	// Returns the game PID
	DWORD GetGamePID(const char* processName);
	// Load informations about the main module in .mainModule
	bool LoadGameMainModule(const char* processName);
public:
	// Status of the process attachment
	GameProcessErrcode_ status{ GameProcessErrcode_PROC_NOT_ATTACHED };
	// Determines if a thread with Update() is currently running
	bool threadStarted{ false };
	// pid of process we latch on
	DWORD processId{ (DWORD)-1 };
	// Stores informations about the main module (address, size, name, exe path)
	moduleEntry mainModule;
	// List of allocated blocks within the game's memory
	std::vector<std::pair<gameAddr, uint64_t>> allocatedMemory;
	// Base address. Every read & write will be done on base + addr.
	//gameAddr baseAddress{ (gameAddr)0x0 };

	// -- Interaction stuff -- //

	// Attach to a process, return false is failed. See .status for more details.
	bool Attach(const char* processName, DWORD processExtraFlags);
	// Detach from the game,
	void Detach();
	// Returns true if .status = PROC_NOT_ATTACHED. Use CheckRunning() for an actual proper check.
	bool IsAttached() const;
	// Checks if pid still used by process & attempts a read. Updates .status & returns false if it fails.
	bool CheckRunning();
	// Frees previous allocated memory. Set [instant] to false (default) if you want to only free memory older than 10 seconds.
	void FreeOldGameMemory(bool instant = false);
	// Returns a list of module currently loaded in the target remote process
	std::vector<moduleEntry> GetModuleList() const;


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
	void    readBytes(gameAddr addr, void* buf, size_t readSize) const;

	// Writes a value to the remote process's memory, adapting to the value type's size
	template<typename T>
	void write(gameAddr addr, T value)
	{
		WriteProcessMemory(m_processHandle, (LPVOID)addr, (LPCVOID)&T, sizeof(T), nullptr);
	};
	// Reads a value in the remote process's memory, adapting to the expected return value type's size
	template<typename T>
	T read(gameAddr addr) const
	{
		T value{ (T)-1 };
		ReadProcessMemory(m_processHandle, (LPCVOID)addr, (LPVOID)&value, sizeof(T), nullptr);
		return value;
	};

	// Writes a byte to the game
	void writeInt8(gameAddr addr, int8_t value);
	// Writes a short (2B) to the game
	void writeInt16(gameAddr addr, int16_t value);
	// Writes an int (4B) to the game
	void writeInt32(gameAddr addr, int32_t value);
	// Writes an int (8B) to the game
	void writeInt64(gameAddr addr, int64_t value);
	// Writes a byte to the game

	void writeUInt8(gameAddr addr, uint8_t value);
	// Writes a short (2B) to the game
	void writeUInt16(gameAddr addr, uint16_t value);
	// Writes an int (4B) to the game
	void writeUInt32(gameAddr addr, uint32_t value);
	// Writes an int (8B) to the game
	void writeUInt64(gameAddr addr, uint64_t value);

	// Writes a floating point number (4b) to the game
	void writeFloat(gameAddr addr, float value);
	// Writes [bufSize] amounts of bytes to the game
	void writeBytes(gameAddr addr, void* buf, size_t bufSize);

	// Injects a DLL (from a file) inside of the process 
	bool InjectDll(const wchar_t* fullpath);
	// (DOES NOT WORK) Injects a DLL (from memory) inside of the process
	bool ReflectInjectDll(const Byte* dllBytes, uint64_t dllSize);

	// Allocates a certain amount of memory in the game
	gameAddr allocateMem(size_t amount, bool executable=false);
	// Frees memory that we previously allocated. Address must be the exact same as when it was returned by allocateMem().
	void freeMem(gameAddr addr);
	// Create a thread in the remote process. Can wait for the end of the thread and receive the thread exit code
	GameProcessThreadCreation_ createRemoteThread(gameAddr startAddress, uint64_t argument=0, bool waitEnd=false, int32_t* exitCodeThread=nullptr);
};
