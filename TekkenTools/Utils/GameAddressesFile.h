#pragma once

#include <map>
#include <string>
#include <vector>

class GameAddressesFile
{
public:
	static GameAddressesFile& getInstance() {
		// Todo: mutex here or something?
		static GameAddressesFile s_instance;
		return s_instance;
	}

	void LoadFile();
	std::vector<void*> GetAddress(const char* c_addressId);
private:
	GameAddressesFile() = default;
	~GameAddressesFile() = default;
	GameAddressesFile& operator = (const GameAddressesFile&) = delete;
	GameAddressesFile(const GameAddressesFile&) = delete;

	std::map<std::string, std::vector<void*>> m_absolute_pointer_paths;
	std::map<std::string, std::vector<void*>> m_relative_pointer_paths;

	std::vector<void*> ParsePtrPath(std::string path);
};
