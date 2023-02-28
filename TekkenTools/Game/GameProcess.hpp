#pragma once

#include <string>
#include <thread>
#include <windows.h>

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
	GameProcess() = default;
	~GameProcess() = default;
	GameProcess& operator = (const GameProcess&) = delete;
	GameProcess(const GameProcess&) = delete;

	// Stores the handle of the opened process
	HANDLE hProcess = nullptr;
	// Returns the game PID
	DWORD GetGamePID();
	// Load the address of the main module in .modBaseAddr
	bool LoadGameMainModule(DWORD pid);
	// Attach to .processName
	GameProcessError AttachToNamedProcess();
	// Detach from the game
	void DetachFromGame();
	// Attempts a read and detaches from the game if it fails
	bool AttemptRead();
public:
	static GameProcess& getInstance() {
		// Todo: mutex here or something?
		static GameProcess s_instance;
		return s_instance;
	}

	// -- Variables -- //
	// 
	// Status of the process attachment
	GameProcessError errcode{ PROC_NOT_ATTACHED };
	// Determines if a thread with Update() is currently running
	bool threadStarted{ false };
	// pid of process we latch on
	DWORD processId{ (DWORD)-1 };
	// Address of main module in game
	BYTE* modBaseAddr{ (BYTE*)0 };
	// Name of process to latch on to
	std::string processName;

	// -- Process stuff -- //

	// Starts a thread that will seek to attach this class to the game
	void StartAttachingThread();
	// Called regularly in order to find the game's process and detect when it is not running anymore
	void Update();

	// -- Interaction stuff -- //
	
	// Reads a char (1b) from the game in little endian
	int8_t  readInt8(void* addr);
	// Reads a short (2b) from the game in little endian
	int16_t readInt16(void* addr);
	// Reads an int (4b) from the game in little endian
	int32_t readInt32(void* addr);
	// Reads an int (4b) from the game in little endian
	int64_t readInt64(void* addr);
	// Reads a floating point number (4b) from the game in little endian
	float   readFloat(void* addr);
	// Reads readSize amounts of bytes from the game and write them to the provided buffer
	void    readBytes(void* addr, void* buf, size_t readSize);

	// Writes a char (1b) to the game
	void writeInt8(void* addr, int8_t value);
	// Writes a short (2b) to the game
	void writeInt16(void* addr, int16_t value);
	// Writes an int (4b) to the game
	void writeInt32(void* addr, int32_t value);
	// Writes a 8b to the game
	void writeInt64(void* addr, int64_t value);
	// Writes a floating point number (4b) to the game
	void writeFloat(void* addr, float value);
	// Writes bufSize amounts of bytes to the game
	void writeBytes(void* addr, void* buf, size_t bufSize);

	// Allocates a certain amount of memory in the game
	void* allocateMem(size_t amount);
	// Frees memory that we previously allocated. Address must be the exact same as when it was returned by allocateMem().
	void freeMem(void* addr);
};
