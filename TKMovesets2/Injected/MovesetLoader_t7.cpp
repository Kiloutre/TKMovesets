#include "MovesetLoader_t7.hpp"

// Reference to the MovesetLoader
MovesetLoaderT7* g_loader = nullptr;

// -- Game functions -- //

namespace T7Functions
{
	// Called after every loading screen on each player address to write the moveset to their offsets
	typedef uint64_t(*ApplyNewMoveset)(void* player, void* newMoveset);

	// Returns the address of a player from his playerid (this is character-related stuff)
	typedef uint64_t(*GetPlayerFromID)(unsigned int playerId);

	// Returns address of a structure that contains playerid among other things
	typedef uint64_t(*GetTK3447820)();
}

// -- Helpers --
// Try to cache variable addresses when possible instead of calling the helpers too much

/*
static void GetPlayerIDs(unsigned int& playerId, unsigned int& remotePlayerId)
{
	auto structAddr = g_loader->CastFunction<T7Functions::GetTK3447820>("TK__GetTK3447820")();
	playerId = *(unsigned int*)(structAddr + 0x6C);
	remotePlayerId = playerId ^ 1;
}
*/

static uint64_t* GetPlayerList()
{
	uint64_t baseFuncAddr = g_loader->GetFunctionAddr("TK__GetPlayerFromID");

	// Base ourselves on the instructions (lea rcx, ...) to find the address since values are relative form the function start
	uint64_t offset = *(uint32_t*)(baseFuncAddr + 11);
	uint64_t offsetBase = baseFuncAddr + 15;

	return (uint64_t*)(offsetBase + offset);
}

// -- Hook functions --

namespace T7Hooks
{
	uint64_t ApplyNewMoveset(void* player, void* newMoveset)
	{
		DEBUG_LOG("ApplyNewMoveset on player %llx, moveset is %llx\n", (uint64_t)player, (uint64_t)newMoveset);

		// Get player IDs
		unsigned int playerId = g_loader->CastVariable<unsigned int>("gTK_playerid");
		unsigned int remotePlayerId = playerId ^ 1;
		DEBUG_LOG("Local player: %d, remote player: %d\n", playerId, remotePlayerId);

		auto playerList = GetPlayerList();

		for (int i = 0; i < 6; ++i)
		{
			DEBUG_LOG("Player %d: %llx\n", i, playerList[i]);
		}

		//g_loader->CastFunction<T7Functions::GetPlayerFromID>()(0);
		return g_loader->CastTrampoline<T7Functions::ApplyNewMoveset>("TK__ApplyNewMoveset")(player, newMoveset);
	}
}

// -- Hooking init --

void MovesetLoaderT7::InitHooks()
{
	// Set the global reference to us
	g_loader = this;

	// Declare which hooks must absolutely be found to initialize this DLL
	m_requiredSymbols = {
		// Hooks
		"TK__ApplyNewMoveset",
		// Functions
		"TK__GetPlayerFromID",
		"TK__GetTK3447820",
	};

	// Find the functions and register them
	RegisterHook("TK__ApplyNewMoveset", m_moduleName, "str:t7_f_ApplyNewMoveset", (uint64_t)&T7Hooks::ApplyNewMoveset);
	RegisterFunction("TK__GetPlayerFromID", m_moduleName, "str:t7_f_GetPlayerFromID");
	RegisterFunction("TK__GetTK3447820", m_moduleName, "str:t7_f_GetTK3447820");
}

void MovesetLoaderT7::PostInit()
{
	// Compute important variable addresses
	m_variables["gTK_playerList"] = (uint64_t)GetPlayerList();

	{
		// There is an actual player side global variable outside of any structure
		// That isn't it, but this function allows us to get the same value in a more reliable way
		auto structAddr = g_loader->CastFunction<T7Functions::GetTK3447820>("TK__GetTK3447820")();
		m_variables["gTK_playerid"] = structAddr + 0x6C;
	}

	// Apply the hooks that need to be applied immediately
	m_hooks["TK__ApplyNewMoveset"].detour->hook();
}