#include "MovesetLoader_t7.hpp"

bool MovesetLoaderT7::Init()
{
    {
        // Load the shared memory handle
        auto sharedMemName = GetSharedMemoryName();
        DEBUG_LOG("Shared memory name is '%s'\n", sharedMemName);

        m_memoryHandle = CreateFileMapping( INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, SHARED_MEMORY_BUFSIZE, sharedMemName);
        DEBUG_LOG("Shared memory handle is %llx\n", (unsigned long long)m_memoryHandle);
        if (m_memoryHandle == nullptr) {
            return false;
        }

        m_sharedMemPtr = (Byte*)MapViewOfFile(m_memoryHandle, FILE_MAP_ALL_ACCESS, 0, 0, SHARED_MEMORY_BUFSIZE);
        DEBUG_LOG("Shared memory address: %llx\n", (unsigned long long)m_sharedMemPtr);
        if (m_sharedMemPtr == nullptr)
        {
            CloseHandle(m_memoryHandle);
            m_memoryHandle = nullptr;
            return false;
        }

        return true;
    }
}