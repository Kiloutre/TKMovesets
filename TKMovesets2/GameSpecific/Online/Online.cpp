#include <format>
#include <windows.h>

#include "Online.hpp"

Online::Online(GameProcess* process, GameData* game, uint16_t gameId, uint16_t minorVersion) : BaseGameSpecificClass(process, game, gameId, minorVersion)
{
    displayedMovesets = new std::vector<movesetInfo>();
}

Online::~Online()
{
    if (m_injectedDll && m_process->IsAttached())
    {
        for (auto& module : m_process->GetModuleList())
        {
            if (module.name == "MovesetLoader.dll")
            {
                // Tell the DLL to unload itself
                DEBUG_LOG("~Online(): Calling Remote " MOVESET_LOADER_STOP_FUNC "(); \n");
                CallMovesetLoaderFunction(MOVESET_LOADER_STOP_FUNC);
                break;
            }
        }
    }

    if (m_memoryHandle != nullptr) {
        CloseHandle(m_memoryHandle);
    }

    delete displayedMovesets;
}

bool Online::LoadSharedMemory()
{
    auto sharedMemName = GetSharedMemoryName();
    m_memoryHandle = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, sharedMemName);
    if (m_memoryHandle == nullptr) {
        //DEBUG_LOG("Error opening file mapping '%s'\n", sharedMemName);
        return false;
    }
    m_sharedMemPtr = (SharedMemory*)MapViewOfFile(m_memoryHandle, FILE_MAP_ALL_ACCESS, 0, 0, SHARED_MEMORY_BUFSIZE);
    if (m_sharedMemPtr == nullptr) {
        DEBUG_LOG("Error mapping view of file for shared memory '%s'\n", sharedMemName);
        CloseHandle(m_memoryHandle);
        m_memoryHandle = nullptr;
    }
    DEBUG_LOG("LoadSharedMemory(): success, ptr is 0x%llx\n", m_sharedMemPtr);
    return true;
}

bool Online::IsMemoryLoaded()
{
    return m_memoryHandle != nullptr;
}

bool Online::CallMovesetLoaderFunction(const char* functionName, bool waitEnd)
{
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

bool Online::InjectDll()
{
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

    if (!m_process->InjectDll(w_dllPath.c_str())) {
        return false;
    }

    m_injectedDll = true;

    // Load said DLL into our own process so that we can call GetProcAddress 
    return CallMovesetLoaderFunction(MOVESET_LOADER_START_FUNC);
}

void Online::OnMovesetImport(movesetInfo* displayedMoveset, gameAddr movesetAddr, unsigned int playerId)
{
    if (displayedMovesets->size() > playerId) {
        (*displayedMovesets)[playerId] = *displayedMoveset;
    }
    else {
        displayedMovesets->push_back(*displayedMoveset);
    }
    m_sharedMemPtr->player[playerId].custom_moveset_addr = movesetAddr;
}