#include "Online_t7.hpp"

void OnlineT7::Init()
{
    m_sharedMemPtr = (SharedMemT7*)m_orig_sharedMemPtr;
}

void OnlineT7::OnMovesetImport(movesetInfo* displayedMoveset, unsigned int playerId, const s_lastLoaded& lastLoadedMoveset)
{
    if (displayedMovesets->size() > playerId) {
        (*displayedMovesets)[playerId] = *displayedMoveset;
    }
    else {
        displayedMovesets->push_back(*displayedMoveset);
    }

    m_sharedMemPtr->players[playerId].crc32 = lastLoadedMoveset.crc32;
    m_sharedMemPtr->players[playerId].custom_moveset_addr = lastLoadedMoveset.address;
    m_sharedMemPtr->players[playerId].moveset_character_id = lastLoadedMoveset.charId;
    m_sharedMemPtr->players[playerId].is_initialized = false;
}
