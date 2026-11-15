#pragma once
#include "MovesetStructs.h"
#include "GameIDs.hpp"

namespace MovesetLimits {
inline constexpr uint64_t Legacy = 256ULL << 20;
inline constexpr uint64_t RetailRuntime = 1024ULL << 20;
inline constexpr uint64_t MaximumStored = RetailRuntime + RetailRuntime / 255 + 65536;
inline uint64_t Expanded(const TKMovesetHeader& header) {
    // Only the explicit retail runtime-snapshot profile gets the larger budget.
    return header.gameId == GameId_T8 && header.minorVersion == GameVersions::T8::RETAIL &&
        header.header_size >= offsetof(TKMovesetHeader, game_specific_flags) + sizeof(uint64_t) &&
        header.game_specific_flags == (1ULL << 33) && header.block_list_size == 1 ? RetailRuntime : Legacy;
}
inline uint64_t Stored(const TKMovesetHeader& header) {
    return Expanded(header) == RetailRuntime ? MaximumStored : Legacy;
}
inline bool Fits(const TKMovesetHeader& header, uint64_t storedSize) {
    if (storedSize > Stored(header)) return false;
    auto limit = Expanded(header);
    if (!header.isCompressed()) return storedSize <= limit;
    return header.moveset_data_start <= limit && header.moveset_data_size <= limit - header.moveset_data_start;
}
}
