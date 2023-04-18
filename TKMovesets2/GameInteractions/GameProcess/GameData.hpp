#pragma once

#include <map>
#include <string>

#include "GameProcess.hpp"
#include "GameAddressesFile.hpp"

#include "GameAddresses.h"

// Class that allows you to read from the game using only the address identifier from game_address.txt
// Also provides caching of pointer paths. But overall this is still a high-level class aimed at cleaning up code elsewhere and keeping things segmented.
// Todo: store current player datas here for fancy displaying later on
class GameData
{
private:
	// Cached addresses and pointer path so that we don't have to re-compute everything mid-extraction/importation
	std::map<std::string, gameAddr> m_cachedAddresses;
	// The subclass that allows us to read/write on process memory
	GameProcess* m_process;
	
	// Reads a ptr path from an address identifier and return its last pointer
	gameAddr ReadPtrPath(const char* c_addressId);
public:
	// Stores ptr paths, absolute addresses, values and such 
	GameAddressesFile* addrFile;
	// Reads a char (1b) from the game in little endian
	int8_t ReadInt8(const char* c_addressId);
	// Reads a short (2b) from the game in little endian
	int16_t ReadInt16(const char* c_addressId);
	// Reads an int (4b) from the game in little endian
	int32_t ReadInt32(const char* c_addressId);
	// Reads an int (8b) from the game in little endian
	int64_t ReadInt64(const char* c_addressId);
	// Reads a 8b value from the game to be interpreted as a pointer
	gameAddr ReadPtr(const char* c_addressId);
	// Reads a floating point number (4b) from the game in little endian
	float ReadFloat(const char* c_addressId);
	// Reads [readSize] amounts of bytes from the game and write them to the provided buffer
	void ReadBytes(const char* c_addressId, void* buf, size_t readSize);
	// Todo: writing functions

	GameData(GameProcess* process, GameAddressesFile* t_addrFile) : m_process(process), addrFile(t_addrFile) {}
	// Reads the addresses file and compute every address from their pointer path (when possible) to avoid having to do it later
	void CacheAddresses();
};