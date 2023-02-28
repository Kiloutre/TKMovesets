#include <fstream>
#include <sstream>
#include <map>
#include <string>
#include <vector>

#include "GameAddressesFile.hpp"

std::map<std::string, std::vector<void*>> g_absolute_pointer_paths;
std::map<std::string, std::vector<void*>> g_relative_pointer_paths;

std::vector<void*> ParsePtrPath(std::string path)
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

namespace GameAddressesFile
{
	void LoadFile()
	{
		g_absolute_pointer_paths.clear();
		g_relative_pointer_paths.clear();

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
			if (key.length() == 0) {
				continue;
			}

			if (value.rfind("+", 0) == 0) {
				g_relative_pointer_paths[key] = ParsePtrPath(value.substr(1));
			}
			else {
				g_absolute_pointer_paths[key] = ParsePtrPath(value);
			}
		}
	}

	bool IsAddressRelative(const char* c_addressId)
	{
		if (g_relative_pointer_paths.find(c_addressId) != g_relative_pointer_paths.end()) {
			return true;
		}
		return false;
	}

	const std::vector<void*> GetAddress(const char* c_addressId)
	{
		if (g_relative_pointer_paths.find(c_addressId) != g_relative_pointer_paths.end()) {
			return g_relative_pointer_paths[c_addressId];
		}
		if (g_absolute_pointer_paths.find(c_addressId) != g_absolute_pointer_paths.end()) {
			return g_absolute_pointer_paths[c_addressId];
		}
		return std::vector<void*>();
	}

	const std::vector<void*> GetAddress(const char* c_addressId, bool& isRelative)
	{
		if (g_relative_pointer_paths.find(c_addressId) != g_relative_pointer_paths.end()) {
			isRelative = true;
			return g_relative_pointer_paths[c_addressId];
		}
		isRelative = false;
		if (g_absolute_pointer_paths.find(c_addressId) != g_absolute_pointer_paths.end()) {
			return g_absolute_pointer_paths[c_addressId];
		}
		return std::vector<void*>();
	}
}

