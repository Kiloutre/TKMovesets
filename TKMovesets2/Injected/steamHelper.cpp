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


	ISteamUser* SteamUser() { return g_steamHelper.pSteamUser; }
	ISteamFriends* SteamFriends() { return g_steamHelper.pSteamFriends; }
	ISteamMatchmaking* SteamMatchmaking() { return g_steamHelper.pSteamMatchmaking; }
	ISteamNetworking* SteamNetworking() { return g_steamHelper.pSteamNetworking; }
};

bool SteamHelperClass::Init(HMODULE steamModule)
{
	if (steamModule == 0) return false;

	auto f_SteamAPI_IsSteamRunning = ((decltype(&SteamAPI_IsSteamRunning))GetProcAddress(steamModule, "SteamAPI_IsSteamRunning"));
	
	if (!f_SteamAPI_IsSteamRunning || !f_SteamAPI_IsSteamRunning()) {
		return false;
	}

	const auto user = reinterpret_cast<decltype(&SteamUser)>(GetProcAddress(steamModule, "SteamUser"));
	const auto friends = reinterpret_cast<decltype(&SteamFriends)>(GetProcAddress(steamModule, "SteamFriends"));
	const auto matchmaking = reinterpret_cast<decltype(&SteamMatchmaking)>(GetProcAddress(steamModule, "SteamMatchmaking"));
	const auto networking = reinterpret_cast<decltype(&SteamNetworking)>(GetProcAddress(steamModule, "SteamNetworking"));
	if (!user || !friends || !matchmaking || !networking) return false;
	pSteamUser = user();
	pSteamFriends = friends();
	pSteamMatchmaking = matchmaking();
	pSteamNetworking = networking();
	m_isInitialized = pSteamUser && pSteamFriends && pSteamMatchmaking && pSteamNetworking;
	return m_isInitialized;
}