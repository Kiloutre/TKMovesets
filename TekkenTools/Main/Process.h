#pragma once

class Process
{
public:
	bool Attach(); //Better error handling?
	bool IsAttached();

	// High level functions

	// Low level functions
	char readByte(long addr);
	short readShort(long addr);
	int readInt(long addr);
	float readFloat(long addr);
	int readBytes(long addr, char* buf, unsigned int BUF_SIZE);
	long allocateMem(unsigned int size);
	void freeMem(long addr);
};