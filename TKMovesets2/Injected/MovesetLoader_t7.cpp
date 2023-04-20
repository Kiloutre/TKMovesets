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
}

namespace T7Hooks
{
	uint64_t ApplyNewMoveset(void* player, void* newMoveset)
	{
		DEBUG_LOG("ApplyNewMoveset on player %llx, moveset is %llx\n", (uint64_t)player, (uint64_t)newMoveset);
		DEBUG_LOG("g_loader is %llx\n", g_loader);
		// todo: TK__GetPlayer(), maybe?
		// put function signature in game_addresses.txt
		// Get player side also

		return g_loader->CastTrampoline<T7Functions::ApplyNewMoveset>("TK__ApplyNewMoveset")(player, newMoveset);
	}
}

// -- Hooking --

void MovesetLoaderT7::InitHooks()
{
	// Set the global reference to us
	g_loader = this;
	DEBUG_LOG("g_loader is set to %llx\n", g_loader);

	// Declare which hooks must absolutely be found
	m_requiredFunctions = {
		"TK__ApplyNewMoveset",
		"TK__GetPlayerFromID"
	};

	RegisterHook("TK__ApplyNewMoveset", m_moduleName.c_str(), "str:t7_f_ApplyNewMoveset", (uint64_t)&T7Hooks::ApplyNewMoveset);
	RegisterFunction("TK__GetPlayerFromID", m_moduleName.c_str(), "str:t7_f_GetPlayerFromID");
}

void MovesetLoaderT7::PostInit()
{
	// Apply the hooks we need to apply right now
	m_hooks["TK__ApplyNewMoveset"].detour->hook();
}