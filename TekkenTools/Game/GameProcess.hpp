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

	HANDLE hProcess = nullptr;
	DWORD GetGamePID();
	bool LoadGameMainModule(DWORD pid);
	GameProcessError AttachToNamedProcess();
	void DetachFromGame();
	bool AttemptRead();
public:
	static GameProcess& getInstance() {
		// Todo: mutex here or something?
		static GameProcess s_instance;
		return s_instance;
	}

	//Variables
	GameProcessError errcode{ PROC_NOT_ATTACHED };
	bool threadStarted{ false };
	DWORD processId{ (DWORD)-1 };
	BYTE* modBaseAddr{ (BYTE*)0 };
	std::string processName;

	// Process stuff
	void Attach();
	void Update();

	// High level functions

	// Low level functions
	char  readByte(long addr);
	short readShort(long addr);
	int   readInt(long addr);
	float readFloat(long addr);
	void  readBytes(long addr, char* buf, size_t bufSize);

	long allocateMem(size_t amount);
	void freeMem(long addr);
};
