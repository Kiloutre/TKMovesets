#pragma once

#include <mutex>
/*
class Process
{
private:
	static Process* s_instance;
	static std::mutex m_;
	Process() {}

	bool attached = false;
public:
	Process& operator = (const Process&) = delete;
	Process(const Process&) = delete;

	static Process* getInstance() {
		if (!s_instance) {
			std::lock_guard<std::mutex> lock(m_);
			if (!s_instance) s_instance = new Process();
		}
		return s_instance;
	}

	// Process stuff
	bool Attach();

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
*/