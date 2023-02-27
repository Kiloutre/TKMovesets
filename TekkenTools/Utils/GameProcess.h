#pragma once

#include <string>
#include <windows.h>
#include <thread>

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

	HANDLE hProcess = NULL;
	DWORD GetGamePID();
	void AttachToNamedProcess();
	void DetachFromGame();
public:
	static GameProcess& getInstance() {
		static GameProcess s_instance;
		return s_instance;
	}

	//Variables
	GameProcessError errcode = PROC_NOT_ATTACHED;
	bool threadStarted = false;
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
	int   readBytes(long addr, char* buf, unsigned int BUF_SIZE);

	long allocateMem(unsigned int size);
	void freeMem(long addr);
};
