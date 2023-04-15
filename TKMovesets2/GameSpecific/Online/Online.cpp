#include <format>
#include <windows.h>

#include "Online.hpp"

Online::~Online()
{
    if (m_injectedDll && m_process->IsAttached()) {
        for (auto& module : m_process->GetModuleList())
        {
            if (module.name == "MovesetLoader.dll")
            {
                // Todo: tell the DLL to unload itself
                break;
            }
        }
    }

    if (m_memoryHandle != nullptr) {
        CloseHandle(m_memoryHandle);
    }
}

bool Online::LoadSharedMemory()
{
    auto sharedMemName = GetSharedMemoryName();
    m_memoryHandle = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, sharedMemName);
    if (m_memoryHandle == nullptr) {
        DEBUG_LOG("Error opening file mapping\n");
        return false;
    }
    m_sharedMemPtr = (Byte*)MapViewOfFile(m_memoryHandle, FILE_MAP_ALL_ACCESS, 0, 0, SHARED_MEMORY_BUFSIZE);
    if (m_sharedMemPtr == nullptr) {
        DEBUG_LOG("Error mapping view of file\n");
        CloseHandle(m_memoryHandle);
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

    const char* functionName = "MovesetLoaderStart";
    std::wstring w_dllName = L"MovesetLoader.dll";
    std::wstring w_dllPath = currDirectory + w_dllName;
    std::string dllName = std::string(w_dllName.begin(), w_dllName.end());

    if (!m_process->InjectDll(w_dllPath.c_str())) {
        return false;
    }

    m_injectedDll = true;

    // Load said DLL into our own process so that we can call GetProcAddress 
    auto movesetLoaderLib = LoadLibraryW(w_dllPath.c_str());
    if (movesetLoaderLib == nullptr) {
        DEBUG_LOG("Error while calling LoadLibraryW locally (%S)\n", w_dllPath.c_str());
        return false;
    }

    auto moduleHandle = GetModuleHandleA(dllName.c_str());
    if (moduleHandle == 0) {
        DEBUG_LOG("Failure getting the module handle for '%s'\n", dllName.c_str());
        FreeLibrary(movesetLoaderLib);
        return false;
    }
    DEBUG_LOG("Module handle is %llx\n", moduleHandle);

    gameAddr startAddr = (gameAddr)GetProcAddress(moduleHandle, functionName);
    if (startAddr == 0) {
        DEBUG_LOG("Failed getting function '%s' address in module '%s'\n", functionName, dllName.c_str());
        DEBUG_LAST_ERR();
        FreeLibrary(movesetLoaderLib);
        return false;
    }
    DEBUG_LOG("%s addr is %llx\n", functionName, startAddr);

    // Finally free our own library now that we have obtained our proc address
    FreeLibrary(movesetLoaderLib);

    m_process->createRemoteThread(startAddr);
    DEBUG_LOG("-- DLL Injected & Thread started --\n");
    return true;
}