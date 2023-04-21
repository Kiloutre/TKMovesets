#pragma once

#include "GameAddressesFile.hpp"

// Wrapper of GameAddressesFile that uses gameKey and minorGameKey to obtain the correct value
class GameAddressesWrapper
{
public:
	// Stores ptr paths, absolute addresses, values and such 
	GameAddressesFile* addrFile;
	// Secondary key used to obtain values from the addresses file
	std::string gameKey;
	// Primary key used to obtain values from the addresses file
	std::string minorGameKey;

	// -- Game addresses interfaces -- //
	int64_t GetValue(const char* c_addressId);
	const char* GetString(const char* c_addressId);
	const std::vector<gameAddr>& GetAddress(const char* c_addressId, bool& isRelative);
};