#pragma once

#include <map>
#include <vector>
#include <fstream>
#include <format>

#include "GameTypes.h"

class GameAddressNotFound : public std::exception {
	std::string m_reason;
	std::string m_gameKey;
	std::string m_addressKey;

public:
	GameAddressNotFound(const std::string& reason, const std::string& gameKey, const std::string& addressKey) : m_reason(reason), m_gameKey(gameKey), m_addressKey(addressKey) {}

	std::string what() {
		return std::format("Addresses key: '{}{}_{}' not found\n", m_reason.c_str(), m_gameKey.c_str(), m_addressKey.c_str());
	}
};

struct GameAddresses_GameEntries
{
	// List of absolute pointer paths in the address list file
	std::map<std::string, std::vector<gameAddr> > absolute_pointer_paths;
	// List of relative pointer pahs in the address list file
	std::map<std::string, std::vector<gameAddr> > relative_pointer_paths;
	// List of entries starting with 'val':
	std::map<std::string, int64_t> values;
	// List of entries starting with 'str:'
	std::map<std::string, std::string> strings;
};

class GameAddressesFile
{
private:
	// Addresses entries grouped by game
	std::map <std::string, GameAddresses_GameEntries> m_entries;
	// List of keys in the address list file
	std::vector<std::string> m_keys;

	// Load the addresses from a stream
	void LoadFromStream(std::istream& stream);
public:
	GameAddressesFile();

	// Reload the game addresses file.
	void Reload();
	// Returns a list of every game_address.txt entry (key only).
	const std::vector<std::string>& GetAllKeys();
	// Returns a single numerical value from the file
	int64_t GetValue(const std::string& gameKey, const char* c_addressId);
	// Returns a string from the file
	const char* GetString(const std::string& gameKey, const char* c_addressId);
	// Returns a pointer path, that may rely on the base address or not.
	const std::vector<gameAddr>& GetPtrPath(const std::string& gameKey, const char* c_addressId, bool& isRelative);
};