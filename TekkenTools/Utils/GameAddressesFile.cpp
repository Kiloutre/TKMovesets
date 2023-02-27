#include <fstream>
#include <sstream>
#include <map>
#include <string>
#include <vector>

#include "GameAddressesFile.hpp"

std::vector<void*> GameAddressesFile::ParsePtrPath(std::string path)
{
	std::vector<void*> ptrPath = std::vector<void*>();

	size_t prevPos = 0;
	size_t pos = 0;
	while ((pos = path.find(",", pos)) != std::string::npos) {
		try {
			ptrPath.push_back((void*)std::stoll(path.substr(prevPos, pos), NULL, 16));
		}
		catch (const std::out_of_range& oor) {
			ptrPath.push_back((void*)0);
		}
		pos += 1;
		prevPos = pos;
	}

	try {
		ptrPath.push_back((void*)std::stoll(path.substr(prevPos), NULL, 16));
	}
	catch (const std::out_of_range& oor) {
		ptrPath.push_back((void*)0);
	}

	return ptrPath;
}

void GameAddressesFile::LoadFile()
{
	m_absolute_pointer_paths.clear();
	m_relative_pointer_paths.clear();

	std::ifstream infile;
	std::string line;

	infile.open("game_addresses.txt");
	while (std::getline(infile, line))
	{
		size_t commentStart = line.find_first_of('#');

		size_t idStart = line.find_first_not_of(' ', 0);
		size_t separator = line.find_first_of('=');

		if (separator == std::string::npos) {
			continue;
		}
		if (commentStart != std::string::npos)
		{
			if (commentStart < separator) continue;
			line = line.substr(0, commentStart);
		}

		std::string key = line.substr(idStart, line.find_first_of(" =", idStart));
		std::string value;
		{
			size_t value_start = line.find_first_not_of(" =", separator);
			value = line.substr(value_start, line.find_last_not_of(" ") + 1 - value_start);
		}

		if (value.rfind("+", 0) == 0) {
			m_relative_pointer_paths[key] = ParsePtrPath(value.substr(1));
		}
		else {
			m_absolute_pointer_paths[key] = ParsePtrPath(value);
		}
	}
}

std::vector<void*> GameAddressesFile::GetAddress(const char *c_addressId)
{
	if (m_relative_pointer_paths.count(c_addressId) != 0) {
		return m_relative_pointer_paths[c_addressId];
	}
	if (m_absolute_pointer_paths.count(c_addressId) != 0) {
		return m_absolute_pointer_paths[c_addressId];
	}
	return std::vector<void*>();
}