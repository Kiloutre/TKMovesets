#include "Online_t8.hpp"

OnlineT8::~OnlineT8()
{
    if (resetMemOnDestroy && m_sharedMemPtr && m_process.CheckRunning()) {
        m_sharedMemPtr->locked_in = false;
    }
}

void OnlineT8::Init()
{
    m_sharedMemPtr = (decltype(m_sharedMemPtr))m_orig_sharedMemPtr;
    if (sizeof(decltype(m_sharedMemPtr)) > SHARED_MEMORY_BUFSIZE) {
        DEBUG_LOG("! Size too big or buffer too small for derived Online::m_sharedMemPtr structure type !\n");
        throw;
    }
}

void OnlineT8::OnMovesetImport(movesetInfo* displayedMoveset, unsigned int playerid, const s_lastLoaded& lastLoadedMoveset)
{

}

void OnlineT8::ClearMovesetSelection(unsigned int playerid)
{

}

void OnlineT8::SetLockIn(bool locked, MovesetLoaderMode_ movesetLoaderMode)
{

}

void OnlineT8::ExecuteExtraprop(uint32_t playerid, uint32_t id, uint32_t value)
{

}