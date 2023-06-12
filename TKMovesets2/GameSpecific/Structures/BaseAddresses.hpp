#pragma once

#include "GameInfo.hpp"
#include "GameAddressesFile.hpp"
#include "GameProcess.hpp"

// Get base addr from game_addresses.ini file
uint64_t GetBaseAddrFromFile(const GameInfo* game, const GameAddressesFile* addrFile, const GameProcess* process) {

	if (addrFile->HasKey(game->minorDataString, "base")) {
		return addrFile->GetValue(game->minorDataString, "base");
	}
	else if (game->minorDataString != game->dataString) {
		if (addrFile->HasKey(game->dataString, "base")) {
			return addrFile->GetValue(game->dataString, "base");
		}
	}
	return 0;
}

// RPCS3 base addr is constant
uint64_t GetRPCS3BaseAddr(const GameInfo* game, const GameAddressesFile* addrFile, const GameProcess* process) {
	// Still leave the possibility to set a (non-zero) base in case RPCS3 ever change its behaviour
	auto base = GetBaseAddrFromFile(game, addrFile, process);
	return base == 0 ? 0x300000000 : base;
}

// PCSX2 base addr is constant
uint64_t GetPCSX2BaseAddr(const GameInfo* game, const GameAddressesFile* addrFile, const GameProcess* process) {
	// Still leave the possibility to set a (non-zero) base in case RPCS3 ever change its behaviour
	auto base = GetBaseAddrFromFile(game, addrFile, process);
	return base == 0 ? 0x20000000 : base;
}