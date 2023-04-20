#include "MovesetLoader_t7.hpp"

// Reference to the MovesetLoader
MovesetLoaderT7* g_loader = nullptr;

// -- Game functions -- //

namespace T7Functions
{
	// Called after every loading screen on each player address to write the moveset to their offsets
	typedef uint64_t(*ApplyNewMoveset)(void* player, void* newMoveset);
}

namespace T7Hooks
{
	uint64_t ApplyNewMoveset(void* player, void* newMoveset)
	{
		DEBUG_LOG("ApplyNewMoveset on player %llx, moveset is %llx\n", (uint64_t)player, (uint64_t)newMoveset);
		DEBUG_LOG("g_loader is %llx\n", g_loader);

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
	m_requiredHooks = {
		"TK__ApplyNewMoveset",
	};

	/// TK__ApplyNewMoveset
	{
		// Get the original address
		T7Functions::ApplyNewMoveset TK__ApplyNewMoveset = 0;
		// Initialize and create the ook
		InitHook("TK__ApplyNewMoveset", (uint64_t)TK__ApplyNewMoveset, (uint64_t)&T7Hooks::ApplyNewMoveset);
	}
}