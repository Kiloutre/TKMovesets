#include <thread>
#include <stdint.h>

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

void DLLCONTENT StartMovesetLoader(uint8_t gameId)
{
    MovesetLoader* loader;

    switch (gameId)
    {
    case 0:
        loader = new MovesetLoaderT7;
    default:
        return;
    }

    loader->Mainloop();
    delete loader;
}
