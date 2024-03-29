#include "Online_t7.hpp"

OnlineT7::~OnlineT7()
{
    if (resetMemOnDestroy && m_sharedMemPtr && m_process.CheckRunning()) {
        m_sharedMemPtr->locked_in = false;
    }
}

void OnlineT7::Init()
{
    m_sharedMemPtr = (decltype(m_sharedMemPtr))m_orig_sharedMemPtr;
    if (sizeof(decltype(m_sharedMemPtr)) > SHARED_MEMORY_BUFSIZE) {
        DEBUG_LOG("! Size too big or buffer too small for derived Online::m_sharedMemPtr structure type !\n");
        throw;
    }
}

void OnlineT7::OnMovesetImport(movesetInfo* displayedMoveset, unsigned int playerid, const s_lastLoaded& lastLoadedMoveset)
{
    while (displayedMovesets->size() <= playerid) {
        displayedMovesets->push_back({.size = 0});
    }

    (*displayedMovesets)[playerid] = *displayedMoveset;

    m_sharedMemPtr->players[playerid].crc32 = lastLoadedMoveset.crc32;
    m_sharedMemPtr->players[playerid].custom_moveset_addr = lastLoadedMoveset.address;
    m_sharedMemPtr->players[playerid].custom_moveset_size = lastLoadedMoveset.size;
    m_sharedMemPtr->players[playerid].custom_moveset_original_data_addr = lastLoadedMoveset.originalDataAddress;
    m_sharedMemPtr->players[playerid].custom_moveset_original_data_size = lastLoadedMoveset.originalDataSize;
    m_sharedMemPtr->players[playerid].moveset_character_id = lastLoadedMoveset.charId;
    m_sharedMemPtr->players[playerid].is_initialized = false;
}

void OnlineT7::ClearMovesetSelection(unsigned int playerid)
{
    movesetInfo emptyMoveset{ .size = 0 };

    while (displayedMovesets->size() <= playerid) {
        displayedMovesets->push_back(emptyMoveset);
    }

    (*displayedMovesets)[playerid] = emptyMoveset;
    m_sharedMemPtr->players[playerid].custom_moveset_addr = 0;
}

void OnlineT7::SetLockIn(bool locked, MovesetLoaderMode_ movesetLoaderMode)
{
    m_sharedMemPtr->locked_in = locked;
    m_sharedMemPtr->moveset_loader_mode = movesetLoaderMode;
}

void OnlineT7::ExecuteExtraprop(uint32_t playerid, uint32_t id, uint32_t value)
{
    if (m_sharedMemPtr != nullptr) {
        m_sharedMemPtr->propToPlay = {
            .playerid = playerid,
            .id = id,
            .value = value
        };
        CallMovesetLoaderFunction("ExecuteExtraprop", true);
    }
    else {
        DEBUG_LOG("ExecuteExtraprop(): m_sharedMemPtr is NULL\n");
    }
}