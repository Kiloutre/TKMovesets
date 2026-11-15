#pragma once

#include <thread>
#include <atomic>
#include <mutex>

class ThreadedClass
{
private:
    std::mutex m_lifecycleMutex;
protected:
    std::atomic_bool m_threadStarted = false;
    std::thread m_t;
public:
    virtual ~ThreadedClass();
    virtual void StartThread();
    virtual void Update() = 0;
    virtual void StopThreadAndCleanup();
};
