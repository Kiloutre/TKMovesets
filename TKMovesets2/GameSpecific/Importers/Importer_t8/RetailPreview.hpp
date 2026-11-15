#pragma once
#include "Importer_t8.hpp"
#include <optional>

namespace RetailT8Preview {
struct State {
    gameAddr player=0,root=0,moves=0;
    uint64_t count=0;
    uint32_t character=0;
    RetailT8Runtime::PlayerOffsets offsets;
    std::optional<uint32_t> current;
};
State Read(GameProcess&,GameData&,const GameInfo&,uint8_t slot);
uint32_t Queue(GameProcess&,GameData&,const GameInfo&,uint8_t slot,gameAddr expectedRoot,uint32_t move);
}
