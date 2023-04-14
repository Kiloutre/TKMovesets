#include <format>

#include "Online.hpp"

Online::~Online()
{
    if (m_memoryHandle != nullptr) {
        CloseHandle(m_memoryHandle);
    }
}

bool Online::LoadSharedMemory()
{
    auto sharedMemName = GetSharedMemoryName();
    m_memoryHandle = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, sharedMemName);
    if (m_memoryHandle == nullptr) {
        return false;
    }
    return true;
}

bool Online::IsMemoryLoaded()
{
    return m_memoryHandle != nullptr;
}

bool Online::InjectDll()
{
    const char* DllName = GetDllName();
    std::string dllPath = std::format("./Resources/{}", DllName);

    if (!m_process->InjectDll(dllPath.c_str())) {
        return false;
    }
    
    return true;
}