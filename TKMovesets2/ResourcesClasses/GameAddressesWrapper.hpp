#pragma once

#include "GameAddressesFile.hpp"

// Wrapper of GameAddressesFile that uses gameKey and minorGameKey to obtain the correct value
class GameAddressesWrapper
{
public:
	// Stores ptr paths, absolute addresses, values and such 
	GameAddressesFile* addrFile = nullptr;
	// Secondary key used to obtain values from the addresses file
	std::string gameKey;
	// Primary key used to obtain values from the addresses file
	std::string minorGameKey;

	// -- Game addresses interfaces -- //
	int64_t GetValue(const char* c_addressId) const;
	const char* GetString(const char* c_addressId) const;
	const std::vector<gameAddr>* GetPtrPath(const char* c_addressId, bool& isRelative) const;
	bool HasKey(const char* key) const;

	// Reads a ptr path, not in the remote process but in the local une. Used by injected DLL.
	uint64_t ReadPtrPathInCurrProcess(const char* c_addressId, uint64_t moduleAddress) const;
};