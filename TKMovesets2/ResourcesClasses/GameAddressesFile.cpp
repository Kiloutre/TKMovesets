#include <fstream>
#include <sstream>
#include <map>
#include <string>
#include <vector>
#include <lz4.h>

#include "GameAddressesFile.hpp"
#include "Helpers.hpp"

#include "constants.h"

// Embedded addresses file data
extern "C" const char game_addresses_ini[];
extern "C" const size_t game_addresses_ini_size;

// -- Static helpers -- //


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

static int GetAddressMapStringSize(const std::map <std::string, GameAddresses_GameEntries>& entries)
{
	uint64_t compacted_addr_size = 0;
	for (auto& [entryKey, entryValue] : entries)
	{
		for (auto& [stringKey, stringValue] : entryValue.strings) {
			compacted_addr_size += 4 + entryKey.size() + 1 + stringKey.size() + 1 + stringValue.size() + 1;
		}
		for (auto& [valKey, valValue] : entryValue.values) {
			compacted_addr_size += 4 + entryKey.size() + 1 + valKey.size() + 1 + std::to_string(valValue).size() + 1;
		}

		for (auto& [ptryPathKey, ptrPath] : entryValue.absolute_pointer_paths) {
			compacted_addr_size += entryKey.size() + 1 + ptryPathKey.size() + 1;
			for (auto& val : ptrPath) {
				compacted_addr_size += std::to_string(val).size() + 1;
			}
		}
		for (auto& [ptryPathKey, ptrPath] : entryValue.relative_pointer_paths) {
			compacted_addr_size += entryKey.size() + 1 + ptryPathKey.size() + 1 + 1;
			for (auto& val : ptrPath) {
				compacted_addr_size += std::to_string(val).size() + 1;
			}
		}
	}

	return (int)compacted_addr_size;
}

static int WriteAddressMapToString(const std::map <std::string, GameAddresses_GameEntries>& entries, char* outBuf)
{
	int compacted_addr_size = GetAddressMapStringSize(entries);
	int bufSize = compacted_addr_size + 1;

	char* inBuf = new char[bufSize];
	inBuf[0] = '\0';
	inBuf[compacted_addr_size] = '\0';

	for (auto& [entryKey, entryValue] : entries)
	{
		std::string sPrefix = "str:" + entryKey + "_";
		for (auto& [stringKey, stringValue] : entryValue.strings) {
			strcat_s(inBuf, bufSize, sPrefix.c_str());
			strcat_s(inBuf, bufSize, stringKey.c_str());
			strcat_s(inBuf, bufSize, "=");
			strcat_s(inBuf, bufSize, stringValue.c_str());
			strcat_s(inBuf, bufSize, "\n");
		}

		std::string valPrefix = "val:" + entryKey + "_";
		for (auto& [valKey, valValue] : entryValue.values) {
			strcat_s(inBuf, bufSize, valPrefix.c_str());
			strcat_s(inBuf, bufSize, valKey.c_str());
			strcat_s(inBuf, bufSize, "=");
			strcat_s(inBuf, bufSize, std::to_string(valValue).c_str());
			strcat_s(inBuf, bufSize, "\n");
		}

		std::string prefix = entryKey + "_";
		for (auto& [ptrPathKey, ptrPath] : entryValue.absolute_pointer_paths) {
			strcat_s(inBuf, bufSize, prefix.c_str());
			strcat_s(inBuf, bufSize, ptrPathKey.c_str());
			strcat_s(inBuf, bufSize, "=");

			for (auto& val : ptrPath) {
				strcat_s(inBuf, bufSize, std::to_string(val).c_str());
				strcat_s(inBuf, bufSize, ",");
			}
			inBuf[strlen(inBuf) - 1] = '\n';
		}
		for (auto& [ptrPathKey, ptrPath] : entryValue.relative_pointer_paths) {
			strcat_s(inBuf, bufSize, prefix.c_str());
			strcat_s(inBuf, bufSize, ptrPathKey.c_str());
			strcat_s(inBuf, bufSize, "=+");

			for (auto& val : ptrPath) {
				strcat_s(inBuf, bufSize, std::to_string(val).c_str());
				strcat_s(inBuf, bufSize, ",");
			}
			inBuf[strlen(inBuf) - 1] = '\n';
		}
	}

	;
	int compressedSize = LZ4_compress_default(inBuf, outBuf, bufSize, (int)sizeof((s_GameAddressesSharedMem*)0)->compressedData);

	delete[] inBuf;
	return compressedSize;
}


// -- Public -- //

GameAddressesFile::GameAddressesFile(bool createSharedMem)
{
	if (createSharedMem) {
		Reload();
		LoadToSharedMemory();
	}
	else {
		// Attempt to load shared mem, but it's okay if it fails
		if (!LoadFromSharedMemory()) {
			Reload();
		}
	}
}


GameAddressesFile::~GameAddressesFile()
{
	if (m_sharedMemory) {
		UnmapViewOfFile(m_sharedMemory);
	}

	if (m_memoryHandle) {
		CloseHandle(m_memoryHandle);
	}
}

void GameAddressesFile::Reload()
{
	// Always attempt to prioritize file data, but in cases where it can't be found, load from embedded data instead
	if (Helpers::fileExists(GAME_ADDRESSES_FILE)) {
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
	// Really not used and doesn't seem ever useful
	return m_keys;
}


int64_t GameAddressesFile::GetValue(const std::string& gameKey, const char* c_addressId)
{
	auto& values = m_entries[gameKey].values;
	auto entry = values.find(c_addressId);
	if (entry != values.end()) {
		return entry->second;
	}

#ifdef BUILD_TYPE_DEBUG
	auto ex = GameAddressNotFound("val:", gameKey, c_addressId);
	DEBUG_LOG(ex.what().c_str());
	throw ex;
#endif
	return (int64_t)-1;
}


const char* GameAddressesFile::GetString(const std::string& gameKey, const char* c_addressId)
{
	auto& strings = m_entries[gameKey].strings;
	auto entry = strings.find(c_addressId);
	if (entry != strings.end()) {
		return entry->second.c_str();
	}

#ifdef BUILD_TYPE_DEBUG
	auto ex = GameAddressNotFound("str:", gameKey, c_addressId);
	DEBUG_LOG(ex.what().c_str());
	throw ex;
#endif
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

#ifdef BUILD_TYPE_DEBUG
	auto ex = GameAddressNotFound("", gameKey, c_addressId);
	DEBUG_LOG(ex.what().c_str());
	throw ex;
#endif
	return std::vector<gameAddr>();
}


// -- Private -- //


bool GameAddressesFile::LoadFromSharedMemory()
{
	DEBUG_LOG("GameAddressesFile::LoadFromSharedMemory()\n");

	m_memoryHandle = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, "Local\\TKMovesets2AddrMem");
	if (m_memoryHandle == nullptr) {
		DEBUG_LOG("!! GameAddressesFile::LoadFromSharedMemory() - Failed to open file mapping !!\n");
		return false;
	}

	m_sharedMemory = (s_GameAddressesSharedMem*)MapViewOfFile(m_memoryHandle, FILE_MAP_ALL_ACCESS, 0, 0, SHARED_ADDR_MEMORY_BUFSIZE);
	if (m_sharedMemory == nullptr) {
		DEBUG_LOG("!! GameAddressesFile::LoadFromSharedMemory() - Failed to map view of file !!\n");
		CloseHandle(m_memoryHandle);
		return false;
	}

	DEBUG_LOG("Orig size: %d, compressed size: %d\n", m_sharedMemory->originalSize, m_sharedMemory->compressedSize);

	int bufSize = m_sharedMemory->originalSize + 1;
	char* outBuf = new char[bufSize];
	outBuf[m_sharedMemory->originalSize] = '\0';

	int decompressedSize = LZ4_decompress_safe(m_sharedMemory->compressedData, outBuf, m_sharedMemory->compressedSize, bufSize);
	DEBUG_LOG("GameAddressesFile::LoadFromSharedMemory() - Decompressed: %d vs %d\n", decompressedSize, m_sharedMemory->originalSize);

	if (decompressedSize <= 0) {
		DEBUG_LOG("!! Not loading addresses from shared mem !! \n");
		return false;
	}

	{
		std::stringstream indata(outBuf);
		LoadFromStream(indata);
	}

	delete[] outBuf;
	DEBUG_LOG("GameAddressesFile::LoadFromSharedMemory() -- Success!\n");

	return true;
}


void GameAddressesFile::LoadToSharedMemory()
{
	DEBUG_LOG("GameAddressesFile::LoadToSharedMemory() - Max size is %u\n", SHARED_ADDR_MEMORY_BUFSIZE);
	int compacted_addr_size = GetAddressMapStringSize(m_entries);
	DEBUG_LOG("GameAddressesFile::LoadToSharedMemory(), compacted (no comments, removed spaces, empty lines) = %u\n", compacted_addr_size);

	m_memoryHandle = CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, SHARED_ADDR_MEMORY_BUFSIZE, "Local\\TKMovesets2AddrMem");
	if (m_memoryHandle == nullptr) {
		DEBUG_LOG("!! GameAddressesFile::LoadToSharedMemory() - Failed to create file mapping !!\n");
		return;
	}

	m_sharedMemory = (s_GameAddressesSharedMem*)MapViewOfFile(m_memoryHandle, FILE_MAP_ALL_ACCESS, 0, 0, SHARED_ADDR_MEMORY_BUFSIZE);
	if (m_sharedMemory == nullptr) {
		DEBUG_LOG("!! GameAddressesFile::LoadToSharedMemory() - Failed to map view of file !!\n");
		CloseHandle(m_memoryHandle);
		return;
	}

	int compressedSize = WriteAddressMapToString(m_entries, m_sharedMemory->compressedData);
	DEBUG_LOG("GameAddressesFile::LoadToSharedMemory(), compressed = %d\n", compressedSize);

	if (compacted_addr_size <= 0) {
		UnmapViewOfFile(m_sharedMemory);
		CloseHandle(m_memoryHandle);
		m_sharedMemory = nullptr;
		m_memoryHandle = nullptr;
	}
	else {
		m_sharedMemory->compressedSize = compressedSize;
		m_sharedMemory->originalSize = compacted_addr_size;
	}
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