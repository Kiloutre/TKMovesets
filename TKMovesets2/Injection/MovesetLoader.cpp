#include <thread>
#include <stdint.h>
#include <string>

#include "MovesetLoader.hpp"
#include "MovesetLoader_t7.hpp"

#include "constants.h"

// Store info globally that way repeated calls to MovesetLoaderStart() won't start multiple loaders
HINSTANCE hModule = nullptr;
MovesetLoader* g_loader = nullptr;

MovesetLoader::MovesetLoader()
{
    {
        // Load the shared memory handle
        auto sharedMemName = GetSharedMemoryName();
        DEBUG_LOG("Shared memory name is %s\n", sharedMemName);
        m_memoryHandle = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, sharedMemName);
        DEBUG_LOG("Shared memory handle is %llx\n", m_memoryHandle);
    }
}

MovesetLoader::~MovesetLoader()
{
    CloseHandle(m_memoryHandle);
}

void MovesetLoader::Mainloop()
{
    while (!mustStop)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(GAME_INTERACTION_THREAD_SLEEP_MS));
    }
}

static std::string GetModuleFilenameStr()
{
    TCHAR szFileName[MAX_PATH];

    GetModuleFileName(NULL, szFileName, MAX_PATH);
    return std::string(szFileName);
}

// -- DLL Exported functions -- //

extern "C"
{
    void __declspec(dllexport) MovesetLoaderStart()
    {
        DEBUG_LOG("MovesetLoaderStart\n");

        if (g_loader != nullptr) {
            return;
        }

        std::string processName = GetModuleFilenameStr();
        if (processName == "TekkenGame-Win64-Shipping.exe") {
            g_loader = new MovesetLoaderT7;
        }
        else {
            return;
        }

        g_loader->Mainloop();
        delete g_loader;
    }

    void __declspec(dllexport) MovesetLoaderStop()
    {
        DEBUG_LOG("MovesetLoaderStop\n");

        if (g_loader != nullptr) {
            g_loader->mustStop = true;
        }
        FreeLibraryAndExitThread(hModule, 0);
    }
}

// DLLMAIN & console

void Console()
{
    AllocConsole();
    freopen("CONOUT$", "w", stdout);
    printf("-- Console started --\n");
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    switch (fdwReason)
    {
    case DLL_PROCESS_ATTACH:
        hModule = hinstDLL;
#ifdef BUILD_TYPE_DEBUG
        CreateThread(NULL, 0x1000, (LPTHREAD_START_ROUTINE)Console, NULL, 0, NULL);
#endif
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