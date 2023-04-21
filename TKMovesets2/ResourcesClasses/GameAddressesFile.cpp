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
	std::map <std::string, GameAddresses_GameEntries> entries;

	std::vector<std::string> keys;
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

		std::string gameKey;
		std::string key = line.substr(idStart, line.find_first_of(" =", idStart));
		std::string shortKey;
		std::string value;
		{
			size_t value_start = line.find_first_not_of(" =", separator);
			value = line.substr(value_start, line.find_last_not_of(" ") + 1 - value_start);
		}
		if (key.length() == 0) {
			continue;
		}

		// Compute game key from the addreses key
		{
			size_t start = key.find_first_of(':');
			size_t end;

			if (start == std::string::npos) {
				start = 0;
			}
			else {
				++start;
			}

			end = key.find_first_of('_', start) - start;
			gameKey = key.substr(start, end);
			if (!entries.contains(gameKey)) {
				entries[gameKey] = {};
			}

			// Also remove the game key from the addresses
			shortKey = key.substr(end + start + 1);
		}

		// Insert value into the appropriate map
		if (Helpers::startsWith<std::string>(key, "val:")) {
			entries[gameKey].values[shortKey] = strtoll(value.c_str(), nullptr, 0);

		}
		else if (Helpers::startsWith<std::string>(key, "str:")) {
			std::string filteredString;
			unsigned int idx = 0;
			for (unsigned char c : value)
			{
				if (!isprint(c)) {
					DEBUG_LOG("GameAddresses warning: string '%s' contains non-printable char '%u' (0x%x) at index %u. Removing it.\n", key.c_str(), c, c, idx);
				}
				else {
					filteredString += (char)c;
				}
				++idx;
			}
			entries[gameKey].strings[shortKey] = filteredString;
		}
		else
		{
			if (value.rfind("+", 0) == 0) {
				// Values starting with '+' are relative to the module address
				entries[gameKey].relative_pointer_paths[shortKey] = parsePtrPathString(value.substr(1));
			}
			else {
				entries[gameKey].absolute_pointer_paths[shortKey] = parsePtrPathString(value);
			}
		}

		keys.push_back(key);
	}

	// Replace these only when we have a proper replacement built, because functions running on other threads require these to be completely built at all times
	// So can't clear them at the start and build them little by little.
	m_entries = entries;
	m_keys = keys;
}

void GameAddressesFile::Reload()
{
	// Always attempt to prioritxize file data, but in cases where it can't be found, load from embedded data instead
	if (false && Helpers::fileExists(GAME_ADDRESSES_FILE)) {
		std::ifstream infile(GAME_ADDRESSES_FILE);
		DEBUG_LOG("Found file '" GAME_ADDRESSES_FILE "'\n");
		LoadFromStream(infile);
	}
	else {
		DEBUG_LOG("Could not find '" GAME_ADDRESSES_FILE "' : loading from embedded data.\n");
		std::stringstream indata(game_addresses_ini);
		LoadFromStream(indata);
	}
	DEBUG_LOG("Addresses loaded.\n");
}

const std::vector<std::string>& GameAddressesFile::GetAllKeys()
{	
	return m_keys;
}

int64_t GameAddressesFile::GetValue(const std::string& gameKey, const char* c_addressId)
{
	auto& values = m_entries[gameKey].values;
    auto entry = values.find(c_addressId);
	if (entry != values.end()) {
		return entry->second;
	}

	auto ex = GameAddressNotFound("val:", gameKey, c_addressId);
	DEBUG_LOG(ex.what().c_str());
	throw ex;
	return (int64_t)-1;
}

const char* GameAddressesFile::GetString(const std::string& gameKey, const char* c_addressId)
{
	auto& strings = m_entries[gameKey].strings;
	auto entry = strings.find(c_addressId);
	if (entry != strings.end()) {
		return entry->second.c_str();
	}

	auto ex = GameAddressNotFound("str:", gameKey, c_addressId);
	DEBUG_LOG(ex.what().c_str());
	throw ex;
	return nullptr;
}

const std::vector<gameAddr>& GameAddressesFile::GetPtrPath(const std::string& gameKey, const char* c_addressId, bool& isRelative)
{
    {
		auto& relative_pointer_paths = m_entries[gameKey].relative_pointer_paths;
        auto entry = relative_pointer_paths.find(c_addressId);
        if (entry != relative_pointer_paths.end()) {
            isRelative = true;
            return entry->second;
        }
    }
    {
		auto& absolute_pointer_paths = m_entries[gameKey].absolute_pointer_paths;
        auto entry = absolute_pointer_paths.find(c_addressId);
        isRelative = false;
        if (entry != absolute_pointer_paths.end()) {
            return entry->second;
        }
    }

	auto ex = GameAddressNotFound("", gameKey, c_addressId);
	DEBUG_LOG(ex.what().c_str());
	throw ex;
	return std::vector<gameAddr>();
}
