#pragma once

#include "GameProcess.hpp"
#include "GameAddressesWrapper.hpp"
#include "Helpers.hpp"

class GameInfo;

// Class that allows you to read from the game using only the address identifier from game_address.txt
// Also provides caching of pointer paths. But overall this is still a high-level class aimed at cleaning up code elsewhere and keeping things segmented.
// Todo: store current player datas here for fancy displaying later on, maybe?
class GameData : public GameAddressesWrapper
{
private:
	// The subclass that allows us to read/write on process memory
	GameProcess* m_process;
	// Size of PTRs in remote process. Used to properly read ptr paths.
	unsigned int m_ptrSize = 8;
	// True if the memory must be accessed as big endian
	bool m_bigEndian = false;
	// Stores current game informations
	const GameInfo* currentGame;
	
	// Reads a ptr path from an address identifier and return its last pointer
	gameAddr ReadPtrPath32(const char* c_addressId) const;
public:
	// For games where PTRs are read from a base memory area (like emulators)
	uint64_t baseAddr = 0;

	template <typename T>
	T Read(gameAddr addr) const
	{
		T value = m_process->read<T>(addr);
		if (m_bigEndian) {
			switch (sizeof(T))
			{
			case 8:
				value = BYTESWAP_INT64(value);
				break;
			case 4:
				value = BYTESWAP_INT32(value);
				break;
			case 2:
				value BYTESWAP_INT16(value);
				break;
			default:
				DEBUG_LOG("!! Error: Invalid read of size %u (must be 2/4/8) in GameData::Read (addr %llx) !!\n", sizeof(T), addr);
				return 0;
				break;
			}
		}
		return value;
	}

	// Reads a ptr, reads 4 or 8 bytes depending on the ptr size and takes into account endian
	gameAddr ReadPtr(gameAddr address) const;
	// Reads [readSize] amounts of bytes from the game and write them to the provided buffer
	void ReadBytes(gameAddr address, void* buf, size_t readSize) const;

	// Reads a ptr path
	gameAddr ReadPtrPath(const char* c_addressId) const;
	// Todo: writing functions

	GameData(GameProcess* process, GameAddressesFile* t_addrFile)
	{
		m_process = process;
		addrFile = t_addrFile;
	}

	// Sets the current game and sets up the PTR size
	void SetCurrentGame(const GameInfo* game);
};