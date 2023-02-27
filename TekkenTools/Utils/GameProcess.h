#pragma once

#include <thread>

enum GameProcessError
{
	PROC_NOT_ATTACHED = 0,
	PROC_ATTACHED = 1,
	PROC_NOT_FOUND = 2,
	PROC_ATTACH_ERR = 3,
	PROC_VERSION_MISMATCH = 4,
	PROC_ATTACHING = 5,
	PROC_EXITED = 6,
};

class GameProcess
{
private:
	GameProcess() = default;
	~GameProcess() = default;
	GameProcess& operator = (const GameProcess&) = delete;
	GameProcess(const GameProcess&) = delete;

	void __AttachToNamedProcess__(const char* processName);
public:
	static GameProcess& getInstance() {
		static GameProcess s_instance;
		return s_instance;
	}

	//Variables
	GameProcessError errcode = PROC_NOT_ATTACHED;
	bool threadStarted = false;

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
