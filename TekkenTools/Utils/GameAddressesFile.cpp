#include <fstream>
#include <sstream>
#include <map>
#include <string>
#include <vector>

#include "GameAddressesFile.hpp"

static std::vector<gameAddr> ParsePtrPathString(std::string path)
{
	std::vector<gameAddr> ptrPath = std::vector<gameAddr>();

	size_t prevPos = 0;
	size_t pos = 0;
	while ((pos = path.find(",", pos)) != std::string::npos) {
		try {
			ptrPath.push_back((gameAddr)std::stoll(path.substr(prevPos, pos), NULL, 16));
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
		ptrPath.push_back((gameAddr)std::stoll(path.substr(prevPos), NULL, 16));
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

void GameAddressesFile::Reload()
{
	std::map<std::string, std::vector<gameAddr>> absolute_pointer_paths;
	std::map<std::string, std::vector<gameAddr>> relative_pointer_paths;
	std::vector<std::string> entries;

	std::ifstream infile;
	std::string line;

	infile.open("game_addresses.txt");
	while (std::getline(infile, line))
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
			relative_pointer_paths[key] = ParsePtrPathString(value.substr(1));
		}
		else {
			absolute_pointer_paths[key] = ParsePtrPathString(value);
		}

		entries.push_back(key);
	}

	// Replace these only when we have a proper replacement built, because functions running on other threads require these to be completley built at all times
	// So can't clear them at the start and build them little by little.
	m_absolute_pointer_paths = absolute_pointer_paths;
	m_relative_pointer_paths = relative_pointer_paths;
	m_entries = entries;
}

const std::vector<std::string> GameAddressesFile::GetAllEntries()
{
	return m_entries;
}

const int64_t GameAddressesFile::GetSingleValue(const char* c_addressId)
{
	if (m_absolute_pointer_paths.find(c_addressId) != m_absolute_pointer_paths.end()) {
		return (int64_t)m_absolute_pointer_paths[c_addressId][0];
	}
	return (int64_t)-1;
}

const std::vector<gameAddr> GameAddressesFile::GetAddress(const char* c_addressId, bool& isRelative)
{
	if (m_relative_pointer_paths.find(c_addressId) != m_relative_pointer_paths.end()) {
		isRelative = true;
		return m_relative_pointer_paths[c_addressId];
	}
	isRelative = false;
	if (m_absolute_pointer_paths.find(c_addressId) != m_absolute_pointer_paths.end()) {
		return m_absolute_pointer_paths[c_addressId];
	}
	return std::vector<gameAddr>();
}
