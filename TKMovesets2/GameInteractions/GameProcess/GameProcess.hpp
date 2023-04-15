#pragma once

#include <string>
#include <vector>
#include <thread>
#include <utility>
#include <windows.h>

#include "GameAddresses.h"

struct processEntry
{
	std::string name;
	DWORD pid;
};

struct moduleEntry
{
	std::string name;
	gameAddr address;
	uint64_t size;
};

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
	// Load the address of the main module in .moduleAddr
	bool LoadGameMainModule(const char* processName);
public:
	// Status of the process attachment
	GameProcessErrcode_ status{ GameProcessErrcode_PROC_NOT_ATTACHED };
	// Determines if a thread with Update() is currently running
	bool threadStarted{ false };
	// pid of process we latch on
	DWORD processId{ (DWORD)-1 };
	// Address of main module in game
	gameAddr moduleAddr{ (gameAddr)0x0 };
	// Contains the size of the module in bytes
	uint64_t moduleSize{ 0 };
	// List of allocated blocks within the game's memory
	std::vector<std::pair<gameAddr, uint64_t>> allocatedMemory;
	// Base address. Every read & write will be done on base + addr.
	//gameAddr baseAddress{ (gameAddr)0x0 };

	// -- Interaction stuff -- //

	// Attach to a process, return false is failed. See .status for more details.
	bool Attach(const char* processName, DWORD processExtraFlags);
	// Detach from the game,
	void Detach();
	// Returns true if .status = PROC_NOT_ATTACHED
	bool IsAttached();
	// Checks if pid still used by process & attempts a read. Updates .status & returns false if it fails.
	bool CheckRunning();
	// Frees previous allocated memory. Set to false (defult) if you want to only free memory older than 10 seconds.
	void FreeOldGameMemory(bool instant = false);
	// Returns a list of module currently loaded in the target remote process
	std::vector<moduleEntry> GetModuleList();


	// Reads an unsigned byte from the game in little endian
	int8_t  readInt8(gameAddr addr);
	// Reads a short (2B) from the game in little endian
	int16_t readInt16(gameAddr addr);
	// Reads an int (4B) from the game in little endian
	int32_t readInt32(gameAddr addr);
	// Reads an int (8B) from the game in little endian
	int64_t readInt64(gameAddr addr);

	// Reads a byte from the game in little endian
	uint8_t  readUInt8(gameAddr addr);
	// Reads a ushort (2B) from the game in little endian
	uint16_t readUInt16(gameAddr addr);
	// Reads an uint (4B) from the game in little endian
	uint32_t readUInt32(gameAddr addr);
	// Reads an uint (8B) from the game in little endian
	uint64_t readUInt64(gameAddr addr);

	// Reads a floating point number (4b) from the game in little endian
	float   readFloat(gameAddr addr);
	// Reads [readSize] amounts of bytes from the game and write them to the provided buffer
	void    readBytes(gameAddr addr, void* buf, size_t readSize);

	// Writes an integer to an address, automatically detecting the type and size
	template<typename T>
	void writeInteger(gameAddr addr, T value)
	{
		switch (sizeof(T))
		{
		case 4:
			writeInt32(addr, *(int32_t*)&value);
			break;
		case 8:
			writeInt64(addr, *(int64_t*)&value);
			break;
		case 2:
			writeInt16(addr, *(int16_t*)&value);
			break;
		case 1:
			writeInt8(addr, *(int8_t*)&value);
			break;
		default:
			throw;
		}
	};
	// Reads an integer, reading a different amount of bytes depending on the expected return type
	template<typename T>
	T readInteger(gameAddr addr)
	{
		switch (sizeof(T))
		{
		case 4:
			uint32_t value = readUInt32(addr);
			return *(T*)&value;
			break;
		case 8:
			uint64_t value = readUInt64(addr);
			return *(T*)&value;
			break;
		case 2:
			uint16_t value = readUInt16(addr);
			return *(T*)&value;
			break;
		case 1:
			uint8_t value = readUInt8(addr);
			return *(T*)&value;
			break;
		default:
			throw;
		}
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

	// Injects a DLL inside of the process
	bool InjectDll(const wchar_t* fullpath);

	// Allocates a certain amount of memory in the game
	gameAddr allocateMem(size_t amount);
	// Frees memory that we previously allocated. Address must be the exact same as when it was returned by allocateMem().
	void freeMem(gameAddr addr);
	// Create a thread in the remote process. Can wait for the end of the thread and receive the thread exit code
	GameProcessThreadCreation_ createRemoteThread(gameAddr startAddress, uint64_t argument=0, bool waitEnd=false, int32_t* exitCodeThread=nullptr);
};
