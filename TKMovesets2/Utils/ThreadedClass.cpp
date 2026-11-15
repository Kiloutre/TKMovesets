#include "ThreadedClass.hpp"

ThreadedClass::~ThreadedClass()
{
    StopThreadAndCleanup();
}

void ThreadedClass::StartThread()
{
    std::lock_guard lock(m_lifecycleMutex);
    if (m_t.joinable()) return;
    m_threadStarted = true;
    try {
        m_t = std::thread(&ThreadedClass::Update, this);
    } catch (...) {
        m_threadStarted = false;
        throw;
    }
}

void ThreadedClass::StopThreadAndCleanup()
{
    std::lock_guard lock(m_lifecycleMutex);
    m_threadStarted = false;
    if (m_t.joinable()) m_t.join();
}
