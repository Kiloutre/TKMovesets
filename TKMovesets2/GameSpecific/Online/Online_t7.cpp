#include "Online_t7.hpp"

void OnlineT7::Init()
{
    m_sharedMemPtr = (SharedMemT7*)m_orig_sharedMemPtr;
    if (sizeof(decltype(m_sharedMemPtr)) > SHARED_MEMORY_BUFSIZE) {
        DEBUG_LOG("! Size too big or buffer too small for derived Online::m_sharedMemPtr structure type !\n");
        throw;
    }
}

void OnlineT7::OnMovesetImport(movesetInfo* displayedMoveset, unsigned int playerId, const s_lastLoaded& lastLoadedMoveset)
{
    (*displayedMovesets)[playerId] = *displayedMoveset;

    m_sharedMemPtr->players[playerId].crc32 = lastLoadedMoveset.crc32;
    m_sharedMemPtr->players[playerId].custom_moveset_addr = lastLoadedMoveset.address;
    m_sharedMemPtr->players[playerId].moveset_character_id = lastLoadedMoveset.charId;
    m_sharedMemPtr->players[playerId].is_initialized = false;
}

void OnlineT7::ClearMovesetSelection(unsigned int playerid)
{
    movesetInfo emptyMoveset;
    emptyMoveset.size = 0;
    (*displayedMovesets)[playerid] = emptyMoveset;
    m_sharedMemPtr->players[playerid].custom_moveset_addr = 0;
}

void OnlineT7::SetLockIn(bool locked)
{
    m_sharedMemPtr->locked_in = locked;
}