#pragma once

#include <map>
#include <vector>
#include <fstream>
#include <format>
#include <windows.h>

#include "constants.h"
#include "GameTypes.h"

// Structure used to pass addresses to injected DLL in case the embedded ones are obsolete
struct s_GameAddressesSharedMem
{
	uint64_t compressedSize;
	uint64_t originalSize;
	char compressedData[SHARED_ADDR_MEMORY_BUFSIZE - sizeof(int) * 2];
};

class GameAddressNotFound : public std::exception
{
	using std::exception::exception;
};

class GameAddressNotFoundMsg : public std::exception {
	std::string m_reason;
	std::string m_gameKey;
	std::string m_addressKey;

public:
	GameAddressNotFoundMsg(const std::string& reason, const std::string& gameKey, const std::string& addressKey) : m_reason(reason), m_gameKey(gameKey), m_addressKey(addressKey) {}

	std::string what() {
		return std::format("!!! Addresses key: '{}{}_{}' not found !!!\n", m_reason.c_str(), m_gameKey.c_str(), m_addressKey.c_str());
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
	// Shared memory where we will load out
	s_GameAddressesSharedMem* m_sharedMemory = nullptr;
	// Handle to the shared memory
	HANDLE m_memoryHandle = nullptr;

	// Load the addresses from a stream
	void LoadFromStream(std::istream& stream);
public:
	GameAddressesFile(bool createSharedMem=false);
	~GameAddressesFile();

	// Read addresses from shared memory, if it exists. Returns false on failure.
	bool LoadFromSharedMemory();
	// Load the currently loaded game addresses into a shared memory
	void LoadToSharedMemory();
	// Reload the game addresses file.
	void Reload();

	// Returns true if such a key exists (looks at values, strings and ptr paths)
	bool HasKey(const std::string& gameKey, const char* key) const;

	// Returns a list of every game_address.txt entry (key only).
	const std::vector<std::string>& GetAllKeys() const;

	// Returns a single numerical value from the file
	int64_t GetValue(const std::string& gameKey, const char* c_addressId) const;
	bool GetValueEx(const std::string& gameKey, const char* c_addressId, int64_t& out) const;
	// Returns a string from the file
	const char* GetString(const std::string& gameKey, const char* c_addressId) const;
	bool GetStringEx(const std::string& gameKey, const char* c_addressId, const char*& out) const;
	// Returns a pointer path, that may rely on the base address or not.
	const std::vector<gameAddr>& GetPtrPath(const std::string& gameKey, const char* c_addressId, bool& isRelative) const;
	bool GetPtrPathEx(const std::string& gameKey, const char* c_addressId, bool& isRelative, const std::vector<gameAddr>*& out) const;
};