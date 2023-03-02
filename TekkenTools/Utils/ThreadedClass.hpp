#pragma once

#include <thread>

class ThreadedClass
{
protected:
	// Is a thread running
	bool m_threadStarted = false;
	// The thread that will run the Update() function regularly
	std::thread m_t;

public:
	virtual void StartThread();
	virtual void Update() = 0;
	virtual void StopThreadAndCleanup();
};