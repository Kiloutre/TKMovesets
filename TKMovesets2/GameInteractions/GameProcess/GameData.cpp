#include "GameInfo.hpp"
#include "GameData.hpp"

#include "GameTypes.h"

gameAddr GameData::ReadPtr(gameAddr address) const
{
	uint64_t value;

	if (m_ptrSize == 8) {
		value = m_process->readUInt64(baseAddr + address);
		if (bigEndian) {
			value = BYTESWAP_INT64(value);
		}
	}
	else {
		value = m_process->readUInt32(baseAddr + address);
		if (bigEndian) {
			value = BYTESWAP_INT32(value);
		}
	}
	return value;
}

gameAddr GameData::ReadPtrPath(const char* c_addressId) const
{
	if (m_ptrSize == 4) {
		return ReadPtrPath32(c_addressId);
	}

	bool isRelative;
	const std::vector<gameAddr>& ptrPath = GetPtrPath(c_addressId, isRelative);

	if (ptrPath.size() == 0) {
		return GAME_ADDR_NULL;
	}

	gameAddr addr = ptrPath[0];
	if (isRelative) {
		addr += m_process->mainModule.address;
	}

	{
		size_t pathLen = ptrPath.size() - 1;
		if (pathLen > 0)
		{
			addr = ReadPtr(baseAddr + addr);
			for (size_t i = 1; i < pathLen; ++i)
			{
				if (addr == (gameAddr)0) {
					return GAME_ADDR_NULL;
				}
				addr = ReadPtr(baseAddr + addr + ptrPath[i]);
			}
			addr += ptrPath[pathLen];
		}
	}

	return addr;
}

gameAddr GameData::ReadPtrPath32(const char* c_addressId) const
{
	bool isRelative;
	const std::vector<gameAddr>& ptrPath = GetPtrPath(c_addressId, isRelative);

	if (ptrPath.size() == 0) {
		return GAME_ADDR_NULL;
	}

	gameAddr32 addr = (gameAddr32)ptrPath[0];
	if (isRelative) {
		addr += (gameAddr32)m_process->mainModule.address;
	}

	{
		size_t pathLen = ptrPath.size() - 1;
		if (pathLen > 0)
		{
			addr = (gameAddr32)ReadPtr(baseAddr + addr);
			for (size_t i = 1; i < pathLen; ++i)
			{
				if (addr == (gameAddr32)0) {
					return GAME_ADDR_NULL;
				}
				addr = (gameAddr32)ReadPtr(baseAddr + addr + ptrPath[i]);
			}
			addr += (gameAddr32)ptrPath[pathLen];
		}
	}

	return addr;
}

void GameData::ReadBytes(gameAddr address, void* buf, size_t readSize) const
{
	m_process->readBytes(baseAddr + address, buf, readSize);
}

void GameData::SetCurrentGame(const GameInfo* game)
{
	// Resets base addr for every game change
	baseAddr = game->GetBaseAddressFunc == nullptr ? 0 : game->GetBaseAddressFunc(game, addrFile, m_process);

	bigEndian = game->bigEndian;
	currentGame = game;
	m_ptrSize = game->ptrSize;
}