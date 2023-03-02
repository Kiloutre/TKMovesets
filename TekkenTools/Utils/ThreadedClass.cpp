#pragma once

#include "ThreadedClass.hpp"


void ThreadedClass::StartThread()
{
	if (!m_threadStarted)
	{
		m_threadStarted = true;
		m_t = std::thread(&ThreadedClass::Update, this);
	}
}

void ThreadedClass::StopThreadAndCleanup()
{
	if (m_threadStarted)
	{
		m_threadStarted = false;
		m_t.join();
	}
}