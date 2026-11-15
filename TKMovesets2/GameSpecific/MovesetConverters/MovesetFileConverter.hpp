#pragma once

#include "LocalStorage.hpp"
#include "GameIDs.hpp"

bool CanConvertMoveset(uint16_t source, uint16_t target, uint16_t targetVersion);
std::filesystem::path ConvertedMovesetPath(const movesetInfo& info, uint16_t target = GameId_T7);
