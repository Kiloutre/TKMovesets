#pragma once

#include "GameInfo.hpp"
#include "GameAddressesFile.hpp"
#include "GameProcess.hpp"

class MissingBaseAddr : public std::exception {};

// Get base addr from game_addresses.ini file
uint64_t GetBaseAddrFromFile(const GameInfo* game, const GameAddressesFile* addrFile, const GameProcess* process);

// RPCS3 base addr is constant
uint64_t GetRPCS3BaseAddr(const GameInfo* game, const GameAddressesFile* addrFile, const GameProcess* process);

// PCSX2 base addr is constant
uint64_t GetPCSX2BaseAddr(const GameInfo* game, const GameAddressesFile* addrFile, const GameProcess* process);

// PPSSPP base addr is variable
uint64_t GetPPSSPPBaseAddr(const GameInfo* game, const GameAddressesFile* addrFile, const GameProcess* process);