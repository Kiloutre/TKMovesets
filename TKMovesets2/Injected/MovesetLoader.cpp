#include <thread>
#include <stdint.h>
#include <string>

#include "MovesetLoader.hpp"
#include "MovesetLoader_t7.hpp"
#include "InjectionUtils.hpp"

#include "constants.h"

// Store info globally that way repeated calls to MovesetLoaderStart() won't start multiple loaders
HINSTANCE hModule = nullptr;
MovesetLoader* g_loader = nullptr;

// -- Debug console -- //
// Only if compiled in debug mode

void InitConsole()
{
    // Create console
    AllocConsole();
    // Redirect stdout to console
    freopen("CONOUT$", "w", stdout);

    DEBUG_LOG("-- Console started --\n");
}

// -- Static helpers -- //

static std::string GetModuleFilenameStr()
{
    TCHAR szFileName[MAX_PATH];

    GetModuleFileName(NULL, szFileName, MAX_PATH);
    std::string filename = std::string(szFileName);
    filename.erase(0, filename.find_last_of("\\/") + 1);
    return filename;
}

//  -- Moveset Loader -- //

MovesetLoader::~MovesetLoader()
{
    if (m_memoryHandle != nullptr) {
        CloseHandle(m_memoryHandle);
    }

    // Unhook any previously hooked function
    for (auto& hook : m_hooks)
    {
        hook.detour->unHook();
        delete hook.detour;
    }
    m_hooks.clear();
}

void MovesetLoader::Mainloop()
{
    while (!mustStop)
    {
        //
        std::this_thread::sleep_for(std::chrono::milliseconds(GAME_INTERACTION_THREAD_SLEEP_MS));
    }
}

bool MovesetLoader::Init()
{
    // Load the shared memory handle
    auto sharedMemName = GetSharedMemoryName();
    DEBUG_LOG("Shared memory name is '%s'\n", sharedMemName);

    m_memoryHandle = CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, SHARED_MEMORY_BUFSIZE, sharedMemName);
    DEBUG_LOG("Shared memory handle is %llx\n", (unsigned long long)m_memoryHandle);
    if (m_memoryHandle == nullptr) {
        return false;
    }

    m_sharedMemPtr = (SharedMemory*)MapViewOfFile(m_memoryHandle, FILE_MAP_ALL_ACCESS, 0, 0, SHARED_MEMORY_BUFSIZE);
    DEBUG_LOG("Shared memory address: %llx\n", (unsigned long long)m_sharedMemPtr);
    if (m_sharedMemPtr == nullptr)
    {
        CloseHandle(m_memoryHandle);
        m_memoryHandle = nullptr;
        return false;
    }

    memset(m_sharedMemPtr, 0, sizeof(*m_sharedMemPtr));
    m_moduleAddr = InjectionUtils::GetSelfModuleAddress(GetMainModuleName());
    DEBUG_LOG("Module '%s' address is %llx\n", GetMainModuleName(), m_moduleAddr);

    PostInit();
    return true;
}

MovesetLoader* MovesetLoader::GetInstance()
{
    return g_loader;
}

// -- DLL Exported functions -- //

extern "C"
{
    void __declspec(dllexport) MovesetLoaderStart()
    {
        // Safety in case this function is called multiple times
        if (g_loader != nullptr && g_loader->isInitialized()) {
            DEBUG_LOG("Loader already started: not starting again\n");
            return;
        }

        // Display console if debug mode
#ifdef BUILD_TYPE_DEBUG
        {
            InitConsole();
            DEBUG_LOG("MovesetLoaderStart\n");
        }
#endif

        // Most likely g_loader is already null, but if previous initialization has failed, it won't be
        if (g_loader == nullptr)
        {
            std::string processName = GetModuleFilenameStr();
            if (processName == "TekkenGame-Win64-Shipping.exe") {
                g_loader = new MovesetLoaderT7;
            }
            else {
                return;
            }
        }

        // Start loop only if Init() succeeds
        // Mainloop will run until process is closed or if MovesetLoaderStop() is called externally
        if (g_loader->Init()) {
            DEBUG_LOG("Starting mainloop.\n");
            g_loader->Mainloop();
            DEBUG_LOG("Mainloop stopped. Unloading the DLL.\n");
            // If mainloop is stopped then we have no reason to let the DLL stay loaded
            delete g_loader;
            g_loader = nullptr;
            FreeLibraryAndExitThread(hModule, 0);
        }
        else {
            DEBUG_LOG("Error during MovesetLoader::Init()\n");
            delete g_loader;
            g_loader = nullptr;
        }
    }

    void __declspec(dllexport) MovesetLoaderStop()
    {
        // Clean way of stopping the loader
        DEBUG_LOG("MovesetLoaderStop\n");

        if (g_loader != nullptr) {
            g_loader->mustStop = true;
        }
    }
}

// DLLMAIN

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    switch (fdwReason)
    {
    case DLL_PROCESS_ATTACH:
        hModule = hinstDLL;
        break;

    case DLL_THREAD_ATTACH:
        break;

    case DLL_THREAD_DETACH:
        break;

    case DLL_PROCESS_DETACH: 
        if (lpvReserved != nullptr) {
            break;
        }
        break;
    }
    return TRUE;
}