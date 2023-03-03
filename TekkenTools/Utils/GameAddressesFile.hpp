#pragma once

#include <vector>

#include "GameAddresses.h"

class GameAddressesFile
{
private:
	// List of absolute pointer paths in the address list file
	std::map<std::string, std::vector<gameAddr> > g_absolute_pointer_paths;
	// List of relative pointer pahs in the address list file
	std::map<std::string, std::vector<gameAddr> > g_relative_pointer_paths;
	// List of entry in the address list file
	std::vector<std::string> g_entries;

public:
	GameAddressesFile();

	// Reload the game addresses file
	void Reload();
	// Returns a list of every game_address.txt entry (key only)
	const std::vector<std::string> GetAllEntries();
	// Returns whether or not the given address is a relative pointer path (have to add main module address to first item) or not
	bool IsAddressRelative(const char* c_addressId);
	// Returns a single numerical value from the file
	const int64_t GetSingleValue(const char* c_addressId);
	// Returns a pointer path, that may rely on the base address or not. To use with IsAddressRelative().
	const std::vector<gameAddr> GetAddress(const char* c_addressId);
	// Returns a pointer path, that may rely on the base address or not.
	const std::vector<gameAddr> GetAddress(const char* c_addressId, bool& isRelative);
};