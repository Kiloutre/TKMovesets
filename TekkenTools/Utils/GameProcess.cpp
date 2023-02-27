#include <chrono>
#include <thread>
#include <cstdio>
#include <windows.h>
#include <tlhelp32.h>

#include "GameProcess.h"

// Starts the attaching process in an existing other thread
void GameProcess::Attach()
{
	if (errcode == PROC_ATTACHED)
		throw("Process is already attached : not re-attaching.");
	errcode = PROC_ATTACHING;
	if (!threadStarted)
	{
		threadStarted = true;
		std::thread t(&GameProcess::Update, this);
		t.detach();
	}
}


void GameProcess::__AttachToNamedProcess__(const char* processName)
{

}

void GameProcess::Update()
{
	while (true)
	{
		if (errcode == PROC_ATTACHING)
		{
			__AttachToNamedProcess__("TekkenGame-Win64-Shipping.exe");
			continue;
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
}