#include <sstream>

#include "GameAddressesFile.hpp"
#include "Compression.hpp"
#include "Helpers.hpp"

#include "constants.h"

// Embedded addresses file data
extern "C" const char game_addresses_ini[];
extern "C" const size_t game_addresses_ini_size;

// -- Static helpers -- //

#define MAX_COMPRESSED_GAME_ADDRESSES_SIZE (sizeof(((s_GameAddressesSharedMem*)0)->compressedData))

static std::vector<gameAddr> parsePtrPathString(const std::string& path)
{
	std::vector<gameAddr> ptrPath = std::vector<gameAddr>();

	size_t prevPos = 0;
	size_t pos = 0;
	while ((pos = path.find(",", pos)) != std::string::npos) {
		try {
			ptrPath.push_back((gameAddr)std::stoll(path.substr(prevPos, pos), NULL, 0));
		}
		catch (const std::exception&) {
			// Todo: warn of bad formatting
			ptrPath.push_back((gameAddr)0);
			DEBUG_ERR("Game addr: bad entry '%s'", path.c_str());
		}
		pos += 1;
		prevPos = pos;
	}

	try {
		ptrPath.push_back((gameAddr)std::stoll(path.substr(prevPos), NULL, 0));
	}
	catch (const std::exception&) {
		// Todo: warn of bad formatting
		ptrPath.push_back((gameAddr)0);
		DEBUG_ERR("Game addr: bad vaoue '%s'", path.c_str());
	}

	return ptrPath;
}

// Calculate the strict minimum size to create a string containing every entry in the game addresses file
// This takes only what is stricly necessary in order for parsing to still work afterward
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

// Write a compacted from of each entry in the game addresses file into the given string
static char* WriteAddressMapToString(const std::map <std::string, GameAddresses_GameEntries>& entries, uint64_t& out_size)
{
	int compacted_addr_size = GetAddressMapStringSize(entries);
	int bufSize = compacted_addr_size + 1;

	char* inBuf = new char[bufSize];
	inBuf[0] = '\0';
	inBuf[compacted_addr_size] = '\0';

	// LOL
	for (auto& [entryKey, entryValue] : entries)
	{
		std::string sPrefix = "s:" + entryKey + "_";
		for (auto& [stringKey, stringValue] : entryValue.strings) {
			strcat_s(inBuf, bufSize, sPrefix.c_str());
			strcat_s(inBuf, bufSize, stringKey.c_str());
			strcat_s(inBuf, bufSize, "=");
			strcat_s(inBuf, bufSize, stringValue.c_str());
			strcat_s(inBuf, bufSize, "\n");
		}

		std::string valPrefix = "v:" + entryKey + "_";
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

	char* output = (char*)CompressionUtils::RAW::LZMA::Compress((Byte*)inBuf, compacted_addr_size, out_size);
	delete[] inBuf;
	return output;
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
	// Always attempt to prioritize file data, but in cases where it can't be found, load from embedded data instead as a fallback
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


bool GameAddressesFile::HasKey(const std::string& gameKey, const char* key) const
{
	auto gameEntry = m_entries.find(gameKey);
	if (gameEntry != m_entries.end())
	{
		auto& g = gameEntry->second;
		return g.absolute_pointer_paths.contains(key) ||
			g.relative_pointer_paths.contains(key) ||
			g.values.contains(key) ||
			g.strings.contains(key);
	}
	return false;
}


const std::vector<std::string>& GameAddressesFile::GetAllKeys() const
{
	// Really not used and doesn't seem ever useful
	return m_keys;
}


int64_t GameAddressesFile::GetValue(const std::string& gameKey, const char* c_addressId) const
{
	auto gameEntry = m_entries.find(gameKey);
	if (gameEntry != m_entries.end())
	{
		auto& values = gameEntry->second.values;
		auto entry = values.find(c_addressId);
		if (entry != values.end()) {
			return entry->second;
		}
	}

#ifdef BUILD_TYPE_DEBUG
	auto ex = GameAddressNotFoundMsg("val:", gameKey, c_addressId);
	DEBUG_LOG(ex.what().c_str());
#endif
	return 0;
}


const char* GameAddressesFile::GetString(const std::string& gameKey, const char* c_addressId) const
{
	auto gameEntry = m_entries.find(gameKey);
	if (gameEntry != m_entries.end())
	{
		auto& strings = gameEntry->second.strings;
		auto entry = strings.find(c_addressId);
		if (entry != strings.end()) {
			return entry->second.c_str();
		}
	}

#ifdef BUILD_TYPE_DEBUG
	auto ex = GameAddressNotFoundMsg("str:", gameKey, c_addressId);
	DEBUG_LOG(ex.what().c_str());
#endif
	return "";
}

std::vector<gameAddr> cg_emptyVector;
const std::vector<gameAddr>& GameAddressesFile::GetPtrPath(const std::string& gameKey, const char* c_addressId, bool& isRelative) const
{
	auto gameEntry = m_entries.find(gameKey);
	if (gameEntry != m_entries.end())
	{
		{
			auto& relative_pointer_paths = gameEntry->second.relative_pointer_paths;
			auto entry = relative_pointer_paths.find(c_addressId);
			if (entry != relative_pointer_paths.end()) {
				isRelative = true;
				return entry->second;
			}
		}
		{
			auto& absolute_pointer_paths = gameEntry->second.absolute_pointer_paths;
			auto entry = absolute_pointer_paths.find(c_addressId);
			isRelative = false;
			if (entry != absolute_pointer_paths.end()) {
				return entry->second;
			}
		}
	}

#ifdef BUILD_TYPE_DEBUG
	auto ex = GameAddressNotFoundMsg("", gameKey, c_addressId);
	DEBUG_LOG(ex.what().c_str());
#endif
	return cg_emptyVector;
}


bool GameAddressesFile::GetValueEx(const std::string& gameKey, const char* c_addressId, int64_t& out) const
{
	auto gameEntry = m_entries.find(gameKey);
	if (gameEntry != m_entries.end())
	{
		auto& values = gameEntry->second.values;
		auto entry = values.find(c_addressId);
		if (entry != values.end()) {
			out = entry->second;
			return true;
		}
	}
	return false;
}


bool GameAddressesFile::GetStringEx(const std::string& gameKey, const char* c_addressId, const char*& out) const
{
	auto gameEntry = m_entries.find(gameKey);
	if (gameEntry != m_entries.end())
	{
		auto& strings = gameEntry->second.strings;
		auto entry = strings.find(c_addressId);
		if (entry != strings.end()) {
			out = entry->second.c_str();
			return true;
		}
	}
	return false;
}

bool GameAddressesFile::GetPtrPathEx(const std::string& gameKey, const char* c_addressId, bool& isRelative, const std::vector<gameAddr>*& out) const
{
	auto gameEntry = m_entries.find(gameKey);
	if (gameEntry != m_entries.end())
	{
		{
			auto& relative_pointer_paths = gameEntry->second.relative_pointer_paths;
			auto entry = relative_pointer_paths.find(c_addressId);
			if (entry != relative_pointer_paths.end()) {
				isRelative = true;
				out = &entry->second;
				return true;
			}
		}
		{
			auto& absolute_pointer_paths = gameEntry->second.absolute_pointer_paths;
			auto entry = absolute_pointer_paths.find(c_addressId);
			isRelative = false;
			if (entry != absolute_pointer_paths.end()) {
				out = &entry->second;
				return true;
			}
		}
	}
	return false;
}



// -- Private -- //


bool GameAddressesFile::LoadFromSharedMemory()
{
	DEBUG_LOG("GameAddressesFile::LoadFromSharedMemory()\n");

	m_memoryHandle = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, "Local\\TKMovesets2AddrMem");
	if (m_memoryHandle == nullptr) {
		DEBUG_ERR("GameAddressesFile::LoadFromSharedMemory() - Failed to open file mapping");
		return false;
	}

	m_sharedMemory = (s_GameAddressesSharedMem*)MapViewOfFile(m_memoryHandle, FILE_MAP_ALL_ACCESS, 0, 0, SHARED_ADDR_MEMORY_BUFSIZE);
	if (m_sharedMemory == nullptr) {
		DEBUG_ERR("GameAddressesFile::LoadFromSharedMemory() - Failed to map view of file");
		CloseHandle(m_memoryHandle);
		return false;
	}

	DEBUG_LOG("Orig size: %llu, compressed size: %llu\n", m_sharedMemory->originalSize, m_sharedMemory->compressedSize);

	char* decompressed_data = (char*)CompressionUtils::RAW::LZMA::Decompress((Byte*)m_sharedMemory->compressedData, m_sharedMemory->compressedSize, m_sharedMemory->originalSize);

	if (decompressed_data == nullptr) {
		DEBUG_ERR("Not loading addresses from shared mem");
		return false;
	}

	{
		std::stringstream indata(decompressed_data);
		LoadFromStream(indata);
	}

	delete[] decompressed_data;
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
		DEBUG_ERR("GameAddressesFile::LoadToSharedMemory() - Failed to create file mapping");
		return;
	}

	m_sharedMemory = (s_GameAddressesSharedMem*)MapViewOfFile(m_memoryHandle, FILE_MAP_ALL_ACCESS, 0, 0, SHARED_ADDR_MEMORY_BUFSIZE);
	if (m_sharedMemory == nullptr) {
		DEBUG_ERR("GameAddressesFile::LoadToSharedMemory() - Failed to map view of file");
		CloseHandle(m_memoryHandle);
		return;
	}

	uint64_t compressed_size;
	char* compressed_data = WriteAddressMapToString(m_entries, compressed_size);
	DEBUG_LOG("GameAddressesFile::LoadToSharedMemory(), compressed = %llu\n", compressed_size);

	if (compacted_addr_size <= 0 || compressed_data == nullptr || compressed_size > MAX_COMPRESSED_GAME_ADDRESSES_SIZE) {
		UnmapViewOfFile(m_sharedMemory);
		CloseHandle(m_memoryHandle);
		m_sharedMemory = nullptr;
		m_memoryHandle = nullptr;
	}
	else {
		memcpy(m_sharedMemory->compressedData, compressed_data, compressed_size);
		m_sharedMemory->compressedSize = compressed_size;
		m_sharedMemory->originalSize = compacted_addr_size;
	}

	delete[] compressed_data;
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

		// Insert value into the appropriate map, auto detect base (int / hex)
		if (Helpers::startsWith<std::string>(key, "val:") || Helpers::startsWith<std::string>(key, "v:")) {
			entries[gameKey].values[shortKey] = strtoll(value.c_str(), nullptr, 0);

		}
		else if (Helpers::startsWith<std::string>(key, "str:") || Helpers::startsWith<std::string>(key, "s:")) {
			std::string filteredString;
			unsigned int idx = 0;
			value[value.size()] = '\0'; // Remove 13 (0xD, carriage return) char at the end of each strings. Don't know why it's there but now it isn't.
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