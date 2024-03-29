#pragma once

#include "GameProcess.hpp"
#include "GameAddressesWrapper.hpp"
#include "Helpers.hpp"

class GameInfo;

// Class that allows you to read from the game, applying the base address and endian conversion is needed
// Also allows you to read a ptr path from the addresses file
class GameData : public GameAddressesWrapper
{
private:
	// The subclass that allows us to read/write on process memory
	GameProcess* m_process;
	// Size of PTRs in remote process. Used to properly read ptr paths.
	unsigned int m_ptrSize = 8;
	// Stores current game informations
	const GameInfo* currentGame = nullptr;
	
	// Reads a ptr path from an address identifier and return its last pointer
	gameAddr ReadPtrPath32(const char* c_addressId) const;
public:
	// For games where PTRs are read from a base memory area (like emulators)
	uint64_t baseAddr = 0;
	// True if the memory must be accessed as big endian
	bool bigEndian = false;

	// Reads a value from the game, applying the base address and the proper endian correction if needed
	template <typename T>
	T Read(gameAddr addr) const
	{
		T value = m_process->read<T>(baseAddr + addr);
		if (bigEndian) {
#pragma warning (push)
#pragma warning (disable:4244)
			switch (sizeof(T))
			{
			case 8:
				value = BYTESWAP_INT64(value);
				break;
			case 4:
				value = BYTESWAP_INT32(value);
				break;
			case 2:
				value = BYTESWAP_INT16(value);
				break;
			}
#pragma warning (pop)
		}
		return value;
	}

	// Reads a ptr, reads 4 or 8 bytes depending on the ptr size, taking into account base address and endian
	gameAddr ReadPtr(gameAddr address) const;
	// Reads [readSize] amounts of bytes from the game and write them to the provided buffer, applies base address before reading
	void ReadBytes(gameAddr address, void* buf, size_t readSize) const;

	// Reads a ptr path
	gameAddr ReadPtrPath(const char* c_addressId) const;

	void SetProcess(GameProcess* process, GameAddressesFile* t_addrFile)
	{
		m_process = process;
		addrFile = t_addrFile;
	}

	// Sets the current game and sets up the PTR size
	void SetCurrentGame(const GameInfo* game);
};