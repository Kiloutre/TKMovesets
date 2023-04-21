#include "Online_t7.hpp"

void OnlineT7::Init()
{
    m_sharedMemPtr = (SharedMemT7*)m_orig_sharedMemPtr;
}

void OnlineT7::OnMovesetImport(movesetInfo* displayedMoveset, gameAddr movesetAddr, unsigned int playerId, uint8_t characterId)
{
    if (displayedMovesets->size() > playerId) {
        (*displayedMovesets)[playerId] = *displayedMoveset;
    }
    else {
        displayedMovesets->push_back(*displayedMoveset);
    }

    m_sharedMemPtr->players[playerId].custom_moveset_addr = movesetAddr;
    m_sharedMemPtr->players[playerId].moveset_character_id = characterId;
    m_sharedMemPtr->players[playerId].is_initialized = false;
}
