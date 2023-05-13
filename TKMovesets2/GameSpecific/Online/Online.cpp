#include <format>
#include <windows.h>
#include <lz4.h>
#include <filesystem>

#include "Online.hpp"
#include "Compression.hpp"

// Embedded moveset loader .dll
extern "C" const char TKMovesetLoader[];
extern "C" const size_t TKMovesetLoader_len;
extern "C" const size_t TKMovesetLoader_orig_len;

// -- Unpack embedded DLL -- //

static bool ExtractMovesetLoaderIfNeeded()
{
    {
        // Get rid of the potentially obsolete old .dll
        struct _stat dll_buffer;
        if (_wstat(L"" MOVESET_LOADER_NAME, &dll_buffer) == 0) {

            struct _stat program_buffer;
            _wstat(L"" PROGRAM_FILENAME, &program_buffer);

            if (dll_buffer.st_mtime < program_buffer.st_mtime) {
                DEBUG_LOG("- .dll out of date, deleting it -\n");
                // DLL was created before this .exe, which means it is not up to date
                try {
                    // Unload library from ourself
                    HMODULE h;
                    while ((h = GetModuleHandleA(MOVESET_LOADER_NAME)) != nullptr) {
                        FreeLibrary(h);
                    }
                    std::filesystem::remove(MOVESET_LOADER_NAME);
                }
                catch (std::filesystem::filesystem_error const&) {
                    // If another process is using the obsolete DLL we are trying to inject,
                    // warn that we can't inject.
                    DEBUG_LOG("!! ERROR WHILE TRYING TO REMOVE OLD DLL !!\n");
                    return false;
                }
            }
        }
    }

    // If .dll does not exist, create it
    // Todo: reflective DLL injection that will not need to create a file
    if (!Helpers::fileExists(L"" MOVESET_LOADER_NAME))
    {
        DEBUG_LOG("Did not find '" MOVESET_LOADER_NAME "'\n");

        Byte* dllContent = CompressionUtils::RAW::LZMA::Decompress((Byte*)TKMovesetLoader, (int)TKMovesetLoader_len, (int)TKMovesetLoader_orig_len);

        if (dllContent != nullptr) {
            std::ofstream file(L"" MOVESET_LOADER_NAME, std::ios::binary);
            if (file.fail()) {
                DEBUG_LOG("Failed to open DLL for writing!\n");
                return false;
            }

            file.write((char*)dllContent, TKMovesetLoader_orig_len);
            DEBUG_LOG("Created file '" MOVESET_LOADER_NAME "'\n");
            delete[] dllContent;
        }
        else {
            DEBUG_LOG("Failed to decompress '" MOVESET_LOADER_NAME "'\n");
        }
    }

    if (GetModuleHandleA(MOVESET_LOADER_NAME) == nullptr) {
        // Load the MovesetLoader in our own process so that we can know its function addresses
        HMODULE movesetLoaderLib = LoadLibraryW(L"" MOVESET_LOADER_NAME);
        if (movesetLoaderLib == nullptr) {
            DEBUG_LOG("Error while calling LoadLibraryW(L\"" MOVESET_LOADER_NAME "\");");
            return false;
        }
    }

    return true;
}

// -- Public functions -- //

Online::Online(GameProcess* process, GameData* game, const GameInfo* gameInfo) : BaseGameSpecificClass(process, game, gameInfo)
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
        CallMovesetLoaderFunction(MOVESET_LOADER_STOP_FUNC);
    }
#endif

    if (m_orig_sharedMemPtr != nullptr) {
        UnmapViewOfFile(m_orig_sharedMemPtr);
    }

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
    m_orig_sharedMemPtr = (SharedMemBase*)MapViewOfFile(m_memoryHandle, FILE_MAP_ALL_ACCESS, 0, 0, SHARED_MEMORY_BUFSIZE);
    if (m_orig_sharedMemPtr == nullptr) {
        DEBUG_LOG("Error mapping view of file for shared memory '%s'\n", sharedMemName);
        CloseHandle(m_memoryHandle);
        m_memoryHandle = nullptr;
        return false;
    }
    DEBUG_LOG("LoadSharedMemory(): success, ptr is 0x%p\n", m_orig_sharedMemPtr);
    Init();
    return true;
}

bool Online::IsMemoryLoaded()
{
    return m_memoryHandle != nullptr;
}

bool Online::VerifyDllVersionMismatch()
{
    if (m_orig_sharedMemPtr != nullptr) {
        versionMismatch = strcmp(m_orig_sharedMemPtr->moveset_loader_version, PROGRAM_VERSION) != 0;
    }
    return versionMismatch;
}

bool Online::CallMovesetLoaderFunction(const char* functionName, bool waitEnd)
{
    DEBUG_LOG("Caling remote process '%s'...\n", functionName);
    auto moduleHandle = GetModuleHandleA(MOVESET_LOADER_NAME);
    if (moduleHandle == 0) {
        DEBUG_LOG("Failure getting the module handle for 'TKMovesetLoader.dll'\n");
        return false;
    }
    DEBUG_LOG("Module handle is %p\n", moduleHandle);

    gameAddr startAddr = (gameAddr)GetProcAddress(moduleHandle, functionName);
    if (startAddr == 0) {
        DEBUG_LOG("Failed getting function '%s' address in module 'TKMovesetLoader.dll'\n", functionName);
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
    bool result = false;

    if (ExtractMovesetLoaderIfNeeded())
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
        if (m_process->InjectDll(w_dllPath.c_str()))
        {
            result = CallMovesetLoaderFunction(MOVESET_LOADER_INIT_FUNC, true);
            if (result) {
                result = CallMovesetLoaderFunction(MOVESET_LOADER_RUN_FUNC);
            }

            injectedDll = true;
        }
    }

    isInjecting = false;

    DEBUG_LOG("Online::InjectDll() -> return\n");
    return result;
}

void Online::CallDebugFunction()
{
    if (m_process->IsAttached() && m_process->CheckRunning()) {
        CallMovesetLoaderFunction("MovesetLoaderDebug", true);
    }
}