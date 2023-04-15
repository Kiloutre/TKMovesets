#include <thread>
#include <stdint.h>
#include <string>

#include "MovesetLoader.hpp"
#include "MovesetLoader_t7.hpp"

MovesetLoader::MovesetLoader()
{
    {
        // Load the shared memory handle
        auto sharedMemName = GetSharedMemoryName();
        m_memoryHandle = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, sharedMemName);
    }
}

MovesetLoader::~MovesetLoader()
{
    CloseHandle(m_memoryHandle);
}

void MovesetLoader::Mainloop()
{
    while (true)
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

void DLLCONTENT StartMovesetLoader()
{
    MovesetLoader* loader;

    std::string processName = GetModuleFilenameStr();
    if (processName == "TekkenGame-Win64-Shipping.exe") {
        loader = new MovesetLoaderT7;
    }
    else {
        return;
    }

    loader->Mainloop();
    delete loader;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    switch (fdwReason)
    {
    case DLL_PROCESS_ATTACH:
        MessageBox(
            NULL,
            "Meow from evil.dll!",
            "=^..^=",
            MB_OK
        );
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