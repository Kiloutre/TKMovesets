#pragma once
#include <array>
#include <vector>
#include <string>
#include "GameProcess.hpp"
#include "ExtractorSettings.hpp"
#include "Structs_t7.h"
#include "MovesetStructs.h"

namespace T7Export {
inline constexpr std::array<size_t, 19> RecordSizes = {
    sizeof(StructsT7::Reactions), sizeof(StructsT7::Requirement), sizeof(StructsT7::HitCondition),
    sizeof(StructsT7::Projectile), sizeof(StructsT7::Pushback), sizeof(StructsT7::PushbackExtradata),
    sizeof(StructsT7::Cancel), sizeof(StructsT7::Cancel), sizeof(StructsT7::CancelExtradata),
    sizeof(StructsT7::ExtraMoveProperty), sizeof(StructsT7::OtherMoveProperty), sizeof(StructsT7::OtherMoveProperty),
    sizeof(StructsT7::Move), sizeof(StructsT7::Voiceclip), sizeof(StructsT7::InputSequence),
    sizeof(StructsT7::Input), sizeof(StructsT7::UnknownParryRelated), sizeof(StructsT7::CameraData),
    sizeof(StructsT7::ThrowCamera)
};
struct Snapshot {
    StructsT7::TKMovesetHeaderBlocks blocks{};
    std::vector<Byte> payload;
    std::vector<uint8_t> discardedMotas;
    std::string characterName;
};
// This is a graph read. Only each individual pointer/count list must be contiguous.
Snapshot ReadSnapshot(const GameProcess& process, gameAddr moveset, ExtractSettings settings, gameAddr trainingMovelist = 0);
// `properties`: the custom property list (import profile); nullptr writes the default profile.
std::vector<Byte> Serialize(const Snapshot& snapshot, TKMovesetHeader header, const std::vector<TKMovesetProperty>* properties = nullptr);
std::string ReadCharacterName(const GameProcess& process, gameAddr moveset);
}
