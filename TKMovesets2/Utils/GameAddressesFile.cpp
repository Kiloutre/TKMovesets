#include <fstream>
#include <sstream>
#include <map>
#include <string>
#include <vector>

#include "GameAddressesFile.hpp"
#include "Helpers.hpp"

#include "constants.h"

// Embedded addresses file data
extern "C" const char game_addresses_ini[];
extern "C" const size_t game_addresses_ini_size;

static std::vector<gameAddr> parsePtrPathString(const std::string& path)
{
	std::vector<gameAddr> ptrPath = std::vector<gameAddr>();

	size_t prevPos = 0;
	size_t pos = 0;
	while ((pos = path.find(",", pos)) != std::string::npos) {
		try {
			ptrPath.push_back((gameAddr)std::stoll(path.substr(prevPos, pos), NULL, 0));
		}
		catch (const std::out_of_range& oor) {
			// Todo: warn of bad formatting
			oor;
			ptrPath.push_back((gameAddr)0);
		}
		pos += 1;
		prevPos = pos;
	}

	try {
		ptrPath.push_back((gameAddr)std::stoll(path.substr(prevPos), NULL, 0));
	}
	catch (const std::out_of_range& oor) {
		// Todo: warn of bad formatting
		oor;
		ptrPath.push_back((gameAddr)0);
	}

	return ptrPath;
}

GameAddressesFile::GameAddressesFile()
{
	Reload();
}

void GameAddressesFile::LoadFromStream(std::istream& stream)
{
	std::map<std::string, std::vector<gameAddr>> absolute_pointer_paths;
	std::map<std::string, std::vector<gameAddr>> relative_pointer_paths;
	std::vector<std::string> entries;
	std::string line;

	while (std::getline(stream, line))
	{
		size_t commentStart = line.find_first_of('#');

		size_t idStart = line.find_first_not_of(' ', 0);
		size_t separator = line.find_first_of('=');

		if (separator == std::string::npos) {
			// Not a 'key = value' entry
			continue;
		}
		if (commentStart != std::string::npos)
		{
			if (commentStart < separator) continue;
			// We remove everything in the line before the comment
			line = line.substr(0, commentStart);
		}

		std::string key = line.substr(idStart, line.find_first_of(" =", idStart));
		std::string value;
		{
			size_t value_start = line.find_first_not_of(" =", separator);
			value = line.substr(value_start, line.find_last_not_of(" ") + 1 - value_start);
		}
		if (key.length() == 0) {
			continue;
		}

		if (value.rfind("+", 0) == 0) {
			// Entries starting with '+' are relative to the module address
			relative_pointer_paths[key] = parsePtrPathString(value.substr(1));
		}
		else {
			absolute_pointer_paths[key] = parsePtrPathString(value);
		}

		entries.push_back(key);
	}

	// Replace these only when we have a proper replacement built, because functions running on other threads require these to be completley built at all times
	// So can't clear them at the start and build them little by little.
	m_absolute_pointer_paths = absolute_pointer_paths;
	m_relative_pointer_paths = relative_pointer_paths;

	m_entries_mutex.lock();
	m_entries = entries;
	m_entries_mutex.unlock();
}

void GameAddressesFile::Reload()
{
	// Always attempt to prioritxize file data, but in cases where it can't be found, load from embedded data instead
	if (Helpers::fileExists(GAME_ADDRESSES_FILE)) {
		std::ifstream infile(GAME_ADDRESSES_FILE);
		LoadFromStream(infile);
		DEBUG_LOG("Found file '" GAME_ADDRESSES_FILE "'.n");
	}
	else {
		std::stringstream indata(game_addresses_ini);
		LoadFromStream(indata);
		DEBUG_LOG("Could not find '" GAME_ADDRESSES_FILE "' : Loaded from embedded data.\n");
	}
}

const std::vector<std::string>& GameAddressesFile::GetAllEntries()
{	
	m_entries_mutex.lock();
	return m_entries;
}

void GameAddressesFile::UnlockEntriesMutex()
{
	m_entries_mutex.unlock();
}

const int64_t GameAddressesFile::GetSingleValue(const char* c_addressId)
{
    auto entry = m_absolute_pointer_paths.find(c_addressId);
	if (entry != m_absolute_pointer_paths.end()) {
		return (int64_t)entry->second[0];
	}
	return (int64_t)-1;
}

const std::vector<gameAddr>& GameAddressesFile::GetAddress(const char* c_addressId, bool& isRelative)
{
    {
        auto entry = m_relative_pointer_paths.find(c_addressId);
        if (entry != m_relative_pointer_paths.end()) {
            isRelative = true;
            return entry->second;
        }
    }
    {
        auto entry = m_absolute_pointer_paths.find(c_addressId);
        isRelative = false;
        if (entry != m_absolute_pointer_paths.end()) {
            return entry->second;
        }
    }
	return m_emptyPtrPath;
}
