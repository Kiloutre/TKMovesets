#include <map>
#include <vector>

#include "GameData.hpp"

#include "GameAddresses.h"

void GameData::CacheAddresses()
{
	std::map<std::string, gameAddr> cachedAddresses;

	for (const std::string& addressId : addrFile->GetAllEntries())
	{
		if (addressId.rfind("val_", 0) == 0) {
			// Ignore if not address / ptr path
			continue;
		}

		cachedAddresses[addressId] = ReadPtrPath(addressId.c_str());
	}
	addrFile->UnlockEntriesMutex();
	m_cachedAddresses = cachedAddresses;
}

gameAddr GameData::ReadPtrPath(const char* c_addressId)
{
	bool isRelative;
	const std::vector<gameAddr>& ptrPath = addrFile->GetAddress(c_addressId, isRelative);

	if (ptrPath.size() == 0) {
		return GAME_ADDR_NULL;
	}

	gameAddr addr = ptrPath[0];
	if (isRelative) {
		addr += m_process->moduleAddr;
	}

	{
		size_t pathLen = ptrPath.size() - 1;
		if (pathLen > 0)
		{
			addr = (gameAddr)m_process->readInt64(addr);
			for (size_t i = 1; i < pathLen; ++i)
			{
				if (addr == (gameAddr)0) {
					return GAME_ADDR_NULL;
				}
				addr = (gameAddr)m_process->readInt64(addr + ptrPath[i]);
			}
			addr += ptrPath[pathLen];
		}
	}

	return addr;
}

int8_t GameData::ReadInt8(const char* c_addressId)
{
    auto addrEntry = m_cachedAddresses.find(c_addressId);
	gameAddr addr = (addrEntry == m_cachedAddresses.end())
		? ReadPtrPath(c_addressId) : addrEntry->second;
	return m_process->readInt8(addr);
}

int16_t GameData::ReadInt16(const char* c_addressId)
{
    auto addrEntry = m_cachedAddresses.find(c_addressId);
	gameAddr addr = (addrEntry == m_cachedAddresses.end())
		? ReadPtrPath(c_addressId) : addrEntry->second;
	return m_process->readInt16(addr);
}

int32_t GameData::ReadInt32(const char* c_addressId)
{
    auto addrEntry = m_cachedAddresses.find(c_addressId);
	gameAddr addr = (addrEntry == m_cachedAddresses.end())
		? ReadPtrPath(c_addressId) : addrEntry->second;
	return m_process->readInt32(addr);
}

int64_t GameData::ReadInt64(const char* c_addressId)
{
    auto addrEntry = m_cachedAddresses.find(c_addressId);
	gameAddr addr = (addrEntry == m_cachedAddresses.end())
				? ReadPtrPath(c_addressId) : addrEntry->second;
	return m_process->readInt64(addr);
}

gameAddr GameData::ReadPtr(const char* c_addressId)
{
	return (gameAddr)ReadPtrPath(c_addressId);
}

float GameData::ReadFloat(const char* c_addressId)
{
    auto addrEntry = m_cachedAddresses.find(c_addressId);
	gameAddr addr = (addrEntry == m_cachedAddresses.end())
		? ReadPtrPath(c_addressId) : addrEntry->second;
	return m_process->readFloat(addr);
}

void GameData::ReadBytes(const char* c_addressId, void* buf, size_t readSize)
{
    auto addrEntry = m_cachedAddresses.find(c_addressId);
	gameAddr addr = (addrEntry == m_cachedAddresses.end())
		? ReadPtrPath(c_addressId) : addrEntry->second;
	return m_process->readBytes(addr, buf, readSize);
}