#pragma once

#include <string>
#include <thread>
#include <windows.h>

#include "GameAddresses.h"

enum GameProcessError
{
	// This will happen if the process-attaching thread is not started
	PROC_NOT_ATTACHED = 0,
	// Process not found in the process list
	PROC_NOT_FOUND = 1,
	// Currently searching for the game process or attempting to attach to it
	PROC_ATTACHING = 2,
	// Found but not attached 
	PROC_ATTACH_ERR = 3,
	// Found, attached, version mismatch
	PROC_VERSION_MISMATCH = 4,
	// Attached with no error
	PROC_ATTACHED = 5,
	// Unexpected exit
	PROC_EXITED = 6,
};

class GameProcess
{
private:
	// Stores the handle of the opened process
	HANDLE hProcess = nullptr;

	// Attach to .processName
	GameProcessError AttachToNamedProcess(const char* processName);
	// Returns the game PID
	DWORD GetGamePID(const char* processName);
	// Load the address of the main module in .modBaseAddr
	bool LoadGameMainModule(const char* processName, DWORD pid);
	// Attempts a read and detaches from the game if it fails
	bool AttemptRead();
	// Detach from the game
	void DetachFromGame();
public:
	// Status of the process attachment
	GameProcessError errcode{ PROC_NOT_ATTACHED };
	// Determines if a thread with Update() is currently running
	bool threadStarted{ false };
	// pid of process we latch on
	DWORD processId{ (DWORD)-1 };
	// Address of main module in game
	int64_t modBaseAddr{ 0 };

	// -- Interaction stuff -- //
	// 
	// Attach to a process, return false is failed. See .errcode for more details.
	bool Attach(const char* processName);
	
	// Reads a char (1b) from the game in little endian
	int8_t  readInt8(gameAddr addr);
	// Reads a short (2b) from the game in little endian
	int16_t readInt16(gameAddr addr);
	// Reads an int (4b) from the game in little endian
	int32_t readInt32(gameAddr addr);
	// Reads an int (4b) from the game in little endian
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
	byte* allocateMem(size_t amount);
	// Frees memory that we previously allocated. Address must be the exact same as when it was returned by allocateMem().
	void freeMem(gameAddr addr);
};
