#pragma once

#include <map>
#include <windows.h>

#include "BaseAddresses.hpp"
#include "helpers.hpp"

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

// Cemu reserves the Wii U's whole 32-bit address space as one 4 GB allocation, at a new address on every start
// (Cemu 2.6, 2026-11-18: one allocation of exactly 0x100000000 bytes; guest address X is at base + X)
uint64_t GetCemuBaseAddr(const GameInfo* game, const GameAddressesFile* addrFile, const GameProcess* process) {
	try {
		return GetBaseAddrFromFile(game, addrFile, process);
	}
	catch (const MissingBaseAddr&) {
	}
	if (process == nullptr || process->processId == (DWORD)-1) return 0;
	HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, process->processId);
	if (!h) return 0;
	std::map<uint64_t, uint64_t> allocationEnds;
	MEMORY_BASIC_INFORMATION info;
	for (uint64_t address = 0; VirtualQueryEx(h, (LPCVOID)address, &info, sizeof(info)) == sizeof(info);) {
		const uint64_t start = (uint64_t)info.BaseAddress, end = start + info.RegionSize;
		if (info.State != MEM_FREE && info.AllocationBase) {
			uint64_t& allocEnd = allocationEnds[(uint64_t)info.AllocationBase];
			if (end > allocEnd) allocEnd = end;
		}
		if (end <= address) break;
		address = end;
	}
	CloseHandle(h);
	for (const auto& [base, end] : allocationEnds) {
		if (end - base == 0x100000000ULL) return base;
	}
	DEBUG_LOG("Cemu: no 4 GB guest memory block found\n");
	return 0;
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