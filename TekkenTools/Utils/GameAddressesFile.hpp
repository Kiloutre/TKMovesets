#pragma once

#include <vector>

namespace GameAddressesFile
{
	// Load the game_addresses.txt file for use GetAddress()
	void LoadFile();

	// Returns a list of every game_address.txt entry (key only)
	const std::vector<std::string> GetAllEntries();

	// Returns whether or not the given address is a relative pointer path (have to add main module address to first item) or not
	bool IsAddressRelative(const char* c_addressId);

	// Returns a pointer path, that may rely on the base address or not. To use with IsAddressRelative().
	const std::vector<void*> GetAddress(const char* c_addressId);
	const std::vector<void*> GetAddress(const char* c_addressId, bool& isRelative);
}