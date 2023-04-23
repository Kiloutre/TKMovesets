#include <format>
#include <windows.h>

#include "Online.hpp"

Online::Online(GameProcess* process, GameData* game, uint16_t gameId, uint16_t minorVersion) : BaseGameSpecificClass(process, game, gameId, minorVersion)
{
    displayedMovesets = new std::vector<movesetInfo>();
    m_dllInjectionThread = new std::thread();
}

Online::~Online()
{
#ifndef BUILD_TYPE_DEBUG
    // Unload the DLL from the game if in release-mode
    // Todo: test, see if this works
    if (resetMemOnDestroy && injectedDll && m_process->CheckRunning())
    {
        // Tell the DLL to unload itself
        DEBUG_LOG("~Online(): Calling Remote " MOVESET_LOADER_STOP_FUNC "(); \n");
        CallMovesetLoaderFunction(MOVESET_LOADER_STOP_FUNC);
    }
#endif

    if (m_memoryHandle != nullptr) {
        CloseHandle(m_memoryHandle);
    }

    if (m_startedThread) {
        m_dllInjectionThread->join();
    }
    delete displayedMovesets;
}

bool Online::LoadSharedMemory()
{
    auto sharedMemName = GetSharedMemoryName();
    m_memoryHandle = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, sharedMemName);
    if (m_memoryHandle == nullptr) {
        return false;
    }
    m_orig_sharedMemPtr = (void*)MapViewOfFile(m_memoryHandle, FILE_MAP_ALL_ACCESS, 0, 0, SHARED_MEMORY_BUFSIZE);
    if (m_orig_sharedMemPtr == nullptr) {
        DEBUG_LOG("Error mapping view of file for shared memory '%s'\n", sharedMemName);
        CloseHandle(m_memoryHandle);
        m_memoryHandle = nullptr;
    }
    DEBUG_LOG("LoadSharedMemory(): success, ptr is 0x%llx\n", m_orig_sharedMemPtr);
    Init();
    return true;
}

bool Online::IsMemoryLoaded()
{
    return m_memoryHandle != nullptr;
}

bool Online::CallMovesetLoaderFunction(const char* functionName, bool waitEnd)
{
    DEBUG_LOG("Caling remote process '%s'...\n", functionName);
    auto moduleHandle = GetModuleHandleA(MOVESET_LOADER_NAME);
    if (moduleHandle == 0) {
        DEBUG_LOG("Failure getting the module handle for 'MovesetLoader.dll'\n");
        return false;
    }
    DEBUG_LOG("Module handle is %llx\n", moduleHandle);

    gameAddr startAddr = (gameAddr)GetProcAddress(moduleHandle, functionName);
    if (startAddr == 0) {
        DEBUG_LOG("Failed getting function '%s' address in module 'MovesetLoader.dll'\n", functionName);
        DEBUG_LAST_ERR();
        return false;
    }
    DEBUG_LOG("%s addr is %llx\n", functionName, startAddr);

    auto errcode = m_process->createRemoteThread(startAddr, 0, waitEnd);
    return errcode != GameProcessThreadCreation_Error;
}

void Online::InjectDll()
{
    if (m_startedThread) {
        // Ensure that the previous injection attempt was finished before trying a new one
        m_dllInjectionThread->join();
    }

    isInjecting = true;
    *m_dllInjectionThread = std::thread(&Online::InjectDllAndWaitEnd, this);
    m_startedThread = true;
}

bool Online::InjectDllAndWaitEnd()
{
    DEBUG_LOG("Online::InjectDll()\n");
    std::wstring currDirectory;
    {
        // Get directory of our .exe because this is where the MovesetLoader is located
        wchar_t currPath[MAX_PATH] = { 0 };
        GetModuleFileNameW(nullptr, currPath, MAX_PATH);
        currDirectory = std::wstring(currPath);
    }

    currDirectory.erase(currDirectory.find_last_of(L"\\/") + 1);

    std::wstring w_dllName = L"" MOVESET_LOADER_NAME;
    std::wstring w_dllPath = currDirectory + w_dllName;
    std::string dllName = std::string(w_dllName.begin(), w_dllName.end());


    // Call the functions to initiate and run the moveset loader
    bool result = false;
    if (m_process->InjectDll(w_dllPath.c_str()))
    {
        result = CallMovesetLoaderFunction(MOVESET_LOADER_INIT_FUNC, true);
        if (result) {
            result = CallMovesetLoaderFunction(MOVESET_LOADER_RUN_FUNC);
        }

        injectedDll = true;
        isInjecting = false;

        DEBUG_LOG("Online::InjectDll() -> return\n");
    }

    return result;
}
