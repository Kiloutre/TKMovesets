#pragma once

#include <windows.h>

#include "steam_api.h"

namespace SteamHelper
{
	// Load up the steam variables, returns false on failure (checks IsStreamRunning)
	bool Init(HMODULE module);
	// Returns true if the steam module has been set and the helpers functions are usable
	bool IsInitialized();


	ISteamClient* SteamClient();
	ISteamUser* SteamUser();
	ISteamFriends* SteamFriends();
	ISteamUtils* SteamUtils();
	ISteamMatchmaking* SteamMatchmaking();
	ISteamGameSearch* SteamGameSearch();
	ISteamUserStats* SteamUserStats();
	ISteamApps* SteamApps();
	ISteamMatchmakingServers* SteamMatchmakingServers();
	ISteamNetworking* SteamNetworking();
	ISteamRemoteStorage* SteamRemoteStorage();
	ISteamScreenshots* SteamScreenshots();
	ISteamHTTP* SteamHTTP();
	ISteamController* SteamController();
	ISteamUGC* SteamUGC();
	ISteamAppList* SteamAppList();
	ISteamMusic* SteamMusic();
	ISteamMusicRemote* SteamMusicRemote();
	ISteamHTMLSurface* SteamHTMLSurface();
	ISteamInventory* SteamInventory();
	ISteamVideo* SteamVideo();
	ISteamParentalSettings* SteamParentalSettings();
	ISteamInput* SteamInput();
}

// Helper used to allow compiling of the moveset loader without the steamworks .cpp source code
class SteamHelperClass
{
public:
	// Set the steam_api DLL module in order to make the SteamHelper function work
	bool Init(HMODULE module);
	// Returns true if the steam module has been set and the helpers functions are usable
	bool IsInitialized() const{ return m_isInitialized; }

	ISteamClient* pSteamClient;
	ISteamUser* pSteamUser;
	ISteamFriends* pSteamFriends;
	ISteamUtils* pSteamUtils;
	ISteamMatchmaking* pSteamMatchmaking;
	ISteamGameSearch* pSteamGameSearch;
	ISteamUserStats* pSteamUserStats;
	ISteamApps* pSteamApps;
	ISteamMatchmakingServers* pSteamMatchmakingServers;
	ISteamNetworking* pSteamNetworking;
	ISteamRemoteStorage* pSteamRemoteStorage;
	ISteamScreenshots* pSteamScreenshots;
	ISteamHTTP* pSteamHTTP;
	ISteamController* pController;
	ISteamUGC* pSteamUGC;
	ISteamAppList* pSteamAppList;
	ISteamMusic* pSteamMusic;
	ISteamMusicRemote* pSteamMusicRemote;
	ISteamHTMLSurface* pSteamHTMLSurface;
	ISteamInventory* pSteamInventory;
	ISteamVideo* pSteamVideo;
	ISteamParentalSettings* pSteamParentalSettings;
	ISteamInput* pSteamInput;
private:
	bool m_isInitialized = false;
};
