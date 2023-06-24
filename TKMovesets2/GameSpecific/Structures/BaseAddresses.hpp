#pragma once

#include "GameInfo.hpp"
#include "GameAddressesFile.hpp"
#include "GameProcess.hpp"

class MissingBaseAddr : public std::exception {};

// Get base addr from game_addresses.ini file
uint64_t GetBaseAddrFromFile(const GameInfo* game, const GameAddressesFile* addrFile, const GameProcess* process)
{
	if (addrFile->HasKey(game->minorDataString, "base")) {
		return addrFile->GetValue(game->minorDataString, "base");
	}
	else if (game->minorDataString != game->dataString) {
		if (addrFile->HasKey(game->dataString, "base")) {
			return addrFile->GetValue(game->dataString, "base");
		}
	}
	throw MissingBaseAddr();
}

// RPCS3 base addr is constant
uint64_t GetRPCS3BaseAddr(const GameInfo* game, const GameAddressesFile* addrFile, const GameProcess* process) {
	// Still leave the possibility to set a base in case RPCS3 ever change its behaviour
	try {
		return GetBaseAddrFromFile(game, addrFile, process);
	}
	catch (const MissingBaseAddr&) {
	}
	return 0x300000000;
}

// PCSX2 base addr is constant
uint64_t GetPCSX2BaseAddr(const GameInfo* game, const GameAddressesFile* addrFile, const GameProcess* process) {
	// Still leave the possibility to set a base in case PCSX2 ever change its behaviour
	try {
		return GetBaseAddrFromFile(game, addrFile, process);
	}
	catch (const MissingBaseAddr&) {
	}
	return 0x20000000;
}

// PPSSPP base addr is variable
uint64_t GetPPSSPPBaseAddr(const GameInfo* game, const GameAddressesFile* addrFile, const GameProcess* process) {
	try {
		return GetBaseAddrFromFile(game, addrFile, process);
	}
	catch (const MissingBaseAddr&) {
		DEBUG_LOG("No base address found\n");
	}
	return 0;
}