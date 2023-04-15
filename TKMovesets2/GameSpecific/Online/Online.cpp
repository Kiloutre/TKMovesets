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
    std::wstring currDirectory;
    {
        // Get directory of our .exe because this is where the MovesetLoader is located
        wchar_t currPath[MAX_PATH] = { 0 };
        GetModuleFileNameW(nullptr, currPath, MAX_PATH);
        currDirectory = std::wstring(currPath);
        currDirectory.erase(currDirectory.find_last_of(L"\\/") + 1);
    }

    std::wstring w_dllPath = currDirectory + L"MovesetLoader.dll";
    std::string dllPath = std::string(w_dllPath.begin(), w_dllPath.end());

    if (!m_process->InjectDll(w_dllPath.c_str())) {
        return false;
    }

    auto moduleHandle = GetModuleHandleA(dllPath.c_str());
    if (moduleHandle == 0) {
        DEBUG_LOG("Failure getting the module handle for '%s'\n", dllPath.c_str());
        return false;
    }
    gameAddr startAddr = (gameAddr)GetProcAddress(moduleHandle, "StartMovesetLoader");
    DEBUG_LOG("StartMovesetLoader addr is %llx\n", startAddr);

    m_process->createRemoteThread(startAddr);
    DEBUG_LOG("Thread successfully created\n");
    return true;
}