#pragma once

#include <string>
#include <vector>
#include <thread>
#include <utility>
#include <windows.h>

#include "GameAddresses.h"

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
	// Module addresses and sized of the current process
	std::vector<std::pair<gameAddr, uint64_t>> m_moduleInfos;

	// Attach to .processName
	GameProcessErrcode_ AttachToNamedProcess(const char* processName, DWORD processExtraFlags);
	// Returns the game PID
	DWORD GetGamePID(const char* processName);
	// Load the address of the main module in .moduleAddr
	bool LoadGameMainModule(const char* processName, DWORD pid);
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
	
	// Reads a char (1b) from the game in little endian
	int8_t  readInt8(gameAddr addr);
	// Reads a short (2b) from the game in little endian
	int16_t readInt16(gameAddr addr);
	// Reads an int (4b) from the game in little endian
	int32_t readInt32(gameAddr addr);
	// Reads an int (8b) from the game in little endian
	int64_t readInt64(gameAddr addr);
	// Reads a floating point number (4b) from the game in little endian
	float   readFloat(gameAddr addr);
	// Reads [readSize] amounts of bytes from the game and write them to the provided buffer
	void    readBytes(gameAddr addr, void* buf, size_t readSize);

	// Writes a char (1b) to the game
	void writeInt8(gameAddr addr, int8_t value);
	// Writes a short (2b) to the game
	void writeInt16(gameAddr addr, int16_t value);
	// Writes an int (4b) to the game
	void writeInt32(gameAddr addr, int32_t value);
	// Writes a 8b to the game
	void writeInt64(gameAddr addr, int64_t value);
	// Writes a floating point number (4b) to the game
	void writeFloat(gameAddr addr, float value);
	// Writes [bufSize] amounts of bytes to the game
	void writeBytes(gameAddr addr, void* buf, size_t bufSize);

	// Allocates a certain amount of memory in the game
	gameAddr allocateMem(size_t amount);
	// Frees memory that we previously allocated. Address must be the exact same as when it was returned by allocateMem().
	void freeMem(gameAddr addr);
	// Create a thread in the remote process
	void createRemoteThread(gameAddr startAddress);
};
