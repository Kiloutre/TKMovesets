#pragma once

#include <mutex>
#include <vector>
#include <fstream>

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
	// Mutex used to avoid m_entries iterator crashing because of concurrent m_entries modification
	std::mutex m_entries_mutex;
	// Returned by GetAddress() if address is not found
	std::vector<gameAddr> m_emptyPtrPath;

	// Load the addresses from a stream
	void LoadFromStream(std::istream& stream);
public:
	GameAddressesFile();

	// Reload the game addresses file.
	void Reload();
	// Returns a list of every game_address.txt entry (key only). Locks a mutex that has to be unlocked by UnlockEntriesMutex() after finishing with the variable.
	const std::vector<std::string>& GetAllEntries();
	// Unlocks the mutex locked by GetAllEntries()
	void UnlockEntriesMutex();
	// Returns a single numerical value from the file
	const int64_t GetSingleValue(const char* c_addressId);
	// Returns a pointer path, that may rely on the base address or not.
	const std::vector<gameAddr>& GetAddress(const char* c_addressId, bool& isRelative);
};