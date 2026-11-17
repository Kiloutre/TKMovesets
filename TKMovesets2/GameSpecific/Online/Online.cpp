#include <format>
#include <windows.h>
#include <filesystem>

#include "Online.hpp"
#include "Compression.hpp"
#include "helpers.hpp"
#include <string>

// Embedded moveset loader .dll
#ifndef BUILD_TYPE_DEBUG
extern "C" const char TKMovesetLoader[];
extern "C" const size_t TKMovesetLoader_len;
extern "C" const size_t TKMovesetLoader_orig_len;
#endif

// -- Unpack embedded DLL -- //

static bool ExtractMovesetLoaderIfNeeded()
{
#ifndef BUILD_TYPE_DEBUG
    {
        // Get rid of the potentially obsolete old .dll
        struct _stat dll_buffer;
        if (_wstat(L"" MOVESET_LOADER_NAME, &dll_buffer) == 0) {

            // Stat the running executable itself (it may have been renamed). If that fails, consider the DLL out of date.
            struct _stat program_buffer {};
            bool programStatOk = false;
            {
                wchar_t programPath[MAX_PATH] = { 0 };
                DWORD programPathLength = GetModuleFileNameW(nullptr, programPath, MAX_PATH);
                programStatOk = programPathLength != 0 && programPathLength < MAX_PATH && _wstat(programPath, &program_buffer) == 0;
            }

            if (!programStatOk || dll_buffer.st_mtime < program_buffer.st_mtime || dll_buffer.st_size != TKMovesetLoader_orig_len) {
                DEBUG_LOG("- .dll out of date, deleting it -\n");
                // DLL was created before this .exe, which means it is not up to date
                
                // Unload library from ourself
                HMODULE h;
                while ((h = GetModuleHandleA(MOVESET_LOADER_NAME)) != nullptr) {
                    FreeLibrary(h);
                }
                try {
                    std::filesystem::remove(MOVESET_LOADER_NAME);
                }
                catch (std::filesystem::filesystem_error const&) {
                    // If another process is using the obsolete DLL we are trying to inject,
                    // warn that we can't inject.
                    DEBUG_ERR("ERROR WHILE TRYING TO REMOVE OLD DLL");
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


#endif
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

Online::Online(GameProcess& process, GameData& game, const GameInfo* gameInfo) : BaseGameSpecificClass(process, game, gameInfo)
{
    displayedMovesets = new std::vector<movesetInfo>();
}

Online::~Online()
{
    if (m_dllInjectionThread.joinable()) m_dllInjectionThread.join();
    if (m_dllInjectionThread.joinable()) m_dllInjectionThread.join();
#ifndef BUILD_TYPE_DEBUG
    // Unload the DLL from the game if in release-mode
    // Todo: test, see if this works
    if (resetMemOnDestroy && injectedDll && m_process.CheckRunning())
    {
        // Tell the DLL to unload itself
        CallMovesetLoaderFunction(MOVESET_LOADER_STOP_FUNC);
    }
#endif

    FreeSharedMemory();

    delete displayedMovesets;
}

void Online::FreeSharedMemory()
{
    if (m_orig_sharedMemPtr != nullptr) {
        UnmapViewOfFile(m_orig_sharedMemPtr);
        m_orig_sharedMemPtr = nullptr;
    }

    if (m_memoryHandle != nullptr) {
        CloseHandle(m_memoryHandle);
        m_memoryHandle = nullptr;
    }
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

    {
        // Write program path to shared memory
        // todo: use this in moveset loader
        wchar_t currPath[MAX_PATH] = { 0 };
        GetModuleFileNameW(nullptr, currPath, MAX_PATH);
        std::wstring ws(currPath);
        ws.erase(ws.find_last_of(L"\\"));
        memcpy(m_orig_sharedMemPtr->program_path, ws.c_str(), ws.size() * sizeof(wchar_t));
    }

    Init();
    return true;
}

bool Online::IsMemoryLoaded()
{
    return m_memoryHandle != nullptr;
}

// Checks a (possibly unterminated) shared memory version string against the loader versions this tool can drive:
// MOVESET_LOADER_MIN_VERSION up to our own (a newer loader may have changed things this tool does not know about).
// Empty = loader still initializing, not a mismatch yet.
static bool IsLoaderVersionMismatching(const SharedMemBase* sharedMem)
{
    const std::string version(sharedMem->moveset_loader_version, strnlen(sharedMem->moveset_loader_version, sizeof(sharedMem->moveset_loader_version)));
    if (version.empty()) {
        return false;
    }
    return Helpers::VersionGreater(MOVESET_LOADER_MIN_VERSION, version.c_str()) || Helpers::VersionGreater(version.c_str(), PROGRAM_VERSION);
}

bool Online::VerifyDllVersionMismatch()
{
    if (m_orig_sharedMemPtr != nullptr && IsLoaderVersionMismatching(m_orig_sharedMemPtr)) {
        DEBUG_LOG("VerifyDllVersionMismatch(): the moveset loader in the game is not from version " PROGRAM_VERSION "\n");
        versionMismatch = true;
    }
    return versionMismatch;
}

bool Online::PeekDllVersionMismatch()
{
    HANDLE memoryHandle = OpenFileMapping(FILE_MAP_READ, FALSE, GetSharedMemoryName());
    if (memoryHandle == nullptr) {
        // No loader has initialized the shared memory yet
        return false;
    }

    bool mismatch = false;
    auto sharedMem = (const SharedMemBase*)MapViewOfFile(memoryHandle, FILE_MAP_READ, 0, 0, sizeof(SharedMemBase));
    if (sharedMem != nullptr) {
        mismatch = IsLoaderVersionMismatching(sharedMem);
        UnmapViewOfFile(sharedMem);
    }
    CloseHandle(memoryHandle);
    return mismatch;
}

bool Online::CallMovesetLoaderFunction(const char* functionName, bool waitEnd, uint32_t* outExitCode)
{
    if (versionMismatch) {
        // A loader from another build has other export offsets than our local copy: never call into it
        DEBUG_LOG("Not calling '%s': moveset loader version mismatch\n", functionName);
        return false;
    }

    DEBUG_LOG("Caling remote process '%s'...\n", functionName);
    // The loader the game holds may be another file than our local copy (an older extraction kept loaded by the game,
    // another build of the same version): its own export table gives the right addresses, wherever it was loaded from.
    if (const gameAddr loaded = m_process.RemoteExport(MOVESET_LOADER_NAME, functionName)) {
        int32_t exitCode = 0;
        const auto result = m_process.createRemoteThread(loaded, 0, waitEnd, &exitCode);
        if (outExitCode != nullptr) *outExitCode = (uint32_t)exitCode;
        return waitEnd ? result == GameProcessThreadCreation_FinishedCleanly
            : result == GameProcessThreadCreation_Started || result == GameProcessThreadCreation_FinishedCleanly;
    }
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

    auto remote = m_process.ResolveRemoteFunction(reinterpret_cast<FARPROC>(startAddr));
    if (!remote) return false;
    int32_t exitCode = 0;
    auto result = m_process.createRemoteThread(remote, 0, waitEnd, &exitCode);
    if (outExitCode != nullptr) {
        *outExitCode = (uint32_t)exitCode;
    }
    return result == GameProcessThreadCreation_Started || result == GameProcessThreadCreation_FinishedCleanly;
}

void Online::InjectDll()
{
    if (m_dllInjectionThread.joinable()) {
        // Ensure that the previous injection attempt was finished before trying a new one
        m_dllInjectionThread.join();
    }

    isInjecting = true;
    m_dllInjectionThread = std::thread(&Online::InjectDllAndWaitEnd, this);
}

bool Online::InjectDllAndWaitEnd()
{
    // The injection thread and direct callers (editors) must never initialize the loader concurrently
    std::lock_guard injectionLock(m_injectionMutex);
    bool result = false;

    // A loader the game already holds is used as it is: a fresh extraction is only needed to load one, and fails while the
    // game keeps the old file open (a newer build started next to it), which used to need a game restart.
    const bool alreadyLoaded = m_process.RemoteExport(MOVESET_LOADER_NAME, MOVESET_LOADER_INIT_FUNC) != 0;
    if (alreadyLoaded || ExtractMovesetLoaderIfNeeded())
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
        std::string dllName = Helpers::wstring_to_string(w_dllName);


        // Call the functions to initiate and run the moveset loader
        if (alreadyLoaded || m_process.InjectDll(w_dllPath.c_str()))
        {
            // A loader that already initialized the shared memory tells its version: refuse to call into another build
            if (PeekDllVersionMismatch()) {
                DEBUG_LOG("Online::InjectDll(): the moveset loader in the game is not from version " PROGRAM_VERSION ", not calling it\n");
                versionMismatch = true;
            }

            uint32_t initStatus = MovesetLoaderInitStatus_Failed;
            result = CallMovesetLoaderFunction(MOVESET_LOADER_INIT_FUNC, true, &initStatus);
            if (result) {
                // Older loaders returned a bool, of which only the low byte is defined
                switch (initStatus & 0xFF)
                {
                case MovesetLoaderInitStatus_Started:
                    result = CallMovesetLoaderFunction(MOVESET_LOADER_RUN_FUNC);
                    break;
                case MovesetLoaderInitStatus_AlreadyRunning:
                    // Usable, and its main loop already runs: do not start a second one
                    result = true;
                    break;
                default:
                    result = false;
                    break;
                }
            }

            injectedDll = result;
        }
    }

    isInjecting = false;

    DEBUG_LOG("Online::InjectDll() -> return\n");
    return result;
}

void Online::CallDebugFunction()
{
    if (m_process.IsAttached() && m_process.CheckRunning()) {
        CallMovesetLoaderFunction("MovesetLoaderDebug", true);
    }
}