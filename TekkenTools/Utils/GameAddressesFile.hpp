#pragma once

#include <vector>

#include "GameAddresses.h"

class GameAddressesFile
{
private:
	// List of absolute pointer paths in the address list file
	std::map<std::string, std::vector<gameAddr> > m_absolute_pointer_paths;
	// List of relative pointer pahs in the address list file
	std::map<std::string, std::vector<gameAddr> > m_relative_pointer_paths;
	// List of entry in the address list file
	std::vector<std::string> m_entries;

public:
	GameAddressesFile();

	// Reload the game addresses file. Do not call: the re-assignation of m_entries would cause any thread iterating on it to throw. TODO : fix this to implement live reload (really not important). Mutex?
	void Reload();
	// Returns a list of every game_address.txt entry (key only)
	const std::vector<std::string> GetAllEntries();
	// Returns a single numerical value from the file
	const int64_t GetSingleValue(const char* c_addressId);
	// Returns a pointer path, that may rely on the base address or not.
	const std::vector<gameAddr> GetAddress(const char* c_addressId, bool& isRelative);
};