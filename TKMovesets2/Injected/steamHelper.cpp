#include "steamHelper.hpp"

SteamHelperClass g_steamHelper;

namespace SteamHelper
{
	bool Init(HMODULE module)
	{
		return g_steamHelper.Init(module);
	}

	bool IsInitialized()
	{
		return g_steamHelper.IsInitialized();
	}


	ISteamClient* SteamClient() { return g_steamHelper.pSteamClient; }
	ISteamUser* SteamUser() { return g_steamHelper.pSteamUser; }
	ISteamFriends* SteamFriends() { return g_steamHelper.pSteamFriends; }
	ISteamUtils* SteamUtils() { return g_steamHelper.pSteamUtils; }
	ISteamMatchmaking* SteamMatchmaking() { return g_steamHelper.pSteamMatchmaking; }
	ISteamGameSearch* SteamGameSearch() { return g_steamHelper.pSteamGameSearch; }
	ISteamUserStats* SteamUserStats() { return g_steamHelper.pSteamUserStats; }
	ISteamApps* SteamApps() { return g_steamHelper.pSteamApps; }
	ISteamMatchmakingServers* SteamMatchmakingServers() { return g_steamHelper.pSteamMatchmakingServers; }
	ISteamNetworking* SteamNetworking() { return g_steamHelper.pSteamNetworking; }
	ISteamRemoteStorage* SteamRemoteStorage() { return g_steamHelper.pSteamRemoteStorage; }
	ISteamScreenshots* SteamScreenshots() { return g_steamHelper.pSteamScreenshots; }
	ISteamHTTP* SteamHTTP() { return g_steamHelper.pSteamHTTP; }
	ISteamController* SteamController() { return g_steamHelper.pController; }
	ISteamUGC* SteamUGC() { return g_steamHelper.pSteamUGC; }
	ISteamAppList* SteamAppList() { return g_steamHelper.pSteamAppList; }
	ISteamMusic* SteamMusic() { return g_steamHelper.pSteamMusic; }
	ISteamMusicRemote* SteamMusicRemote() { return g_steamHelper.pSteamMusicRemote; }
	ISteamHTMLSurface* SteamHTMLSurface() { return g_steamHelper.pSteamHTMLSurface; }
	ISteamInventory* SteamInventory() { return g_steamHelper.pSteamInventory; }
	ISteamVideo* SteamVideo() { return g_steamHelper.pSteamVideo; }
	ISteamParentalSettings* SteamParentalSettings() { return g_steamHelper.pSteamParentalSettings; }
	ISteamInput* SteamInput() { return g_steamHelper.pSteamInput; }
};

bool SteamHelperClass::Init(HMODULE steamModule)
{
	if (steamModule == 0) return false;

	auto f_SteamAPI_IsSteamRunning = ((decltype(&SteamAPI_IsSteamRunning))GetProcAddress(steamModule, "SteamAPI_IsSteamRunning"));
	
	if (!f_SteamAPI_IsSteamRunning || !f_SteamAPI_IsSteamRunning()) {
		return false;
	}

	pSteamClient = ((decltype(&SteamClient))GetProcAddress(steamModule, "SteamClient"))();
	pSteamUser = ((decltype(&SteamUser))GetProcAddress(steamModule, "SteamUser"))();
	pSteamFriends = ((decltype(&SteamFriends))GetProcAddress(steamModule, "SteamFriends"))();
	pSteamUtils = ((decltype(&SteamUtils))GetProcAddress(steamModule, "SteamUtils"))();
	pSteamMatchmaking = ((decltype(&SteamMatchmaking))GetProcAddress(steamModule, "SteamMatchmaking"))();
	pSteamGameSearch = nullptr; // This one is not exported by the DLL for some reason. Do not use it, i guess?
	pSteamUserStats = ((decltype(&SteamUserStats))GetProcAddress(steamModule, "SteamUserStats"))();
	pSteamApps = ((decltype(&SteamApps))GetProcAddress(steamModule, "SteamApps"))();
	pSteamMatchmakingServers = ((decltype(&SteamMatchmakingServers))GetProcAddress(steamModule, "SteamMatchmakingServers"))();
	pSteamNetworking = ((decltype(&SteamNetworking))GetProcAddress(steamModule, "SteamNetworking"))();
	pSteamRemoteStorage = ((decltype(&SteamRemoteStorage))GetProcAddress(steamModule, "SteamRemoteStorage"))();
	pSteamScreenshots = ((decltype(&SteamScreenshots))GetProcAddress(steamModule, "SteamScreenshots"))();
	pSteamHTTP = ((decltype(&SteamHTTP))GetProcAddress(steamModule, "SteamHTTP"))();
	pController = ((decltype(&SteamController))GetProcAddress(steamModule, "SteamController"))();
	pSteamUGC = ((decltype(&SteamUGC))GetProcAddress(steamModule, "SteamUGC"))();
	pSteamAppList = ((decltype(&SteamAppList))GetProcAddress(steamModule, "SteamAppList"))();
	pSteamMusic = ((decltype(&SteamMusic))GetProcAddress(steamModule, "SteamMusic"))();
	pSteamMusicRemote = ((decltype(&SteamMusicRemote))GetProcAddress(steamModule, "SteamMusicRemote"))();
	pSteamHTMLSurface = ((decltype(&SteamHTMLSurface))GetProcAddress(steamModule, "SteamHTMLSurface"))();
	pSteamInventory = ((decltype(&SteamInventory))GetProcAddress(steamModule, "SteamInventory"))();
	pSteamVideo = ((decltype(&SteamVideo))GetProcAddress(steamModule, "SteamVideo"))();
	pSteamParentalSettings = nullptr; // This one is not exported by the DLL for some reason. Do not use it, i guess?
	pSteamInput = nullptr; // This one is not exported by the DLL for some reason. Do not use it, i guess?

	m_isInitialized = true;
	return true;
}