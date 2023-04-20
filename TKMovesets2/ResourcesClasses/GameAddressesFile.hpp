#pragma once

#include <vector>
#include <fstream>

#include "GameTypes.h"

class GameAddressesFile
{
private:
	// List of absolute pointer paths in the address list file
	std::map<std::string, std::vector<gameAddr> > m_absolute_pointer_paths;
	// List of relative pointer pahs in the address list file
	std::map<std::string, std::vector<gameAddr> > m_relative_pointer_paths;
	// List of entries starting with 'val':
	std::map<std::string, int64_t> m_values;
	// List of entries starting with 'str:'
	std::map<std::string, std::string> m_strings;

	// List of entry in the address list file
	std::vector<std::string> m_entries;

	// Load the addresses from a stream
	void LoadFromStream(std::istream& stream);
public:
	GameAddressesFile();

	// Reload the game addresses file.
	void Reload();
	// Returns a list of every game_address.txt entry (key only).
	const std::vector<std::string>& GetAllEntries();
	// Returns a single numerical value from the file
	int64_t GetValue(const char* c_addressId);
	// Returns a string from the file
	const char* GetString(const char* c_addressId);
	// Returns a pointer path, that may rely on the base address or not.
	const std::vector<gameAddr>& GetAddress(const char* c_addressId, bool& isRelative);
};