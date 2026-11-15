#pragma once

#include <windows.h>

#include "steam_api.h"

namespace SteamHelper
{
	// Load up the steam variables, returns false on failure (checks IsStreamRunning)
	bool Init(HMODULE module);
	// Returns true if the steam module has been set and the helpers functions are usable
	bool IsInitialized();


	ISteamUser* SteamUser();
	ISteamFriends* SteamFriends();
	ISteamMatchmaking* SteamMatchmaking();
	ISteamNetworking* SteamNetworking();
}

// Helper used to allow compiling of the moveset loader without the steamworks .cpp source code
class SteamHelperClass
{
public:
	// Set the steam_api DLL module in order to make the SteamHelper function work
	bool Init(HMODULE module);
	// Returns true if the steam module has been set and the helpers functions are usable
	bool IsInitialized() const{ return m_isInitialized; }

	ISteamUser* pSteamUser;
	ISteamFriends* pSteamFriends;
	ISteamMatchmaking* pSteamMatchmaking;
	ISteamNetworking* pSteamNetworking;
private:
	bool m_isInitialized = false;
};
