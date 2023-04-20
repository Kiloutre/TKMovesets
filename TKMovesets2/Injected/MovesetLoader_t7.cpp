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
		const char* TK__ApplyNewMoveset_bytes = "48 85 D2 0F 84 CF 01 00 00 56 57 48 83 EC 28 48 89 5C 24 40 48 8B FA 48 89 6C 24 48 48 8B F1 48 89 91 20 15 00 00 48 89 91 28 15 00 00 48 89 91";
		T7Functions::ApplyNewMoveset TK__ApplyNewMoveset = (T7Functions::ApplyNewMoveset)Sig::find((void*)m_moduleAddr, m_moduleSize, TK__ApplyNewMoveset_bytes);
		// Initialize and create the ook
		InitHook("TK__ApplyNewMoveset", (uint64_t)TK__ApplyNewMoveset, (uint64_t)&T7Hooks::ApplyNewMoveset);
	}
}

void MovesetLoaderT7::PostInit()
{
	m_hooks["TK__ApplyNewMoveset"].detour->hook();
}