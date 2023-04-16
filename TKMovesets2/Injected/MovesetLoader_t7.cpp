#include "MovesetLoader_t7.hpp"

// -- Game functions -- //

namespace T7Functions
{
	// Called after every loading screen on each player address to write the moveset to their offsets
	typedef uint64_t(*ApplyNewMoveset)(void* player, void* newMoveset);
}

//
T7Functions::ApplyNewMoveset TK__ApplyNewMoveset; // Orig function address
uint64_t oTK__ApplyNewMoveset = 0; // Trampoline
//

namespace T7Hooks
{
	uint64_t ApplyNewMoveset(void* player, void* newMoveset)
	{
		DEBUG_LOG("ApplyNewMoveset on player %llx, moveset is %llx\n", (uint64_t)player, (uint64_t)newMoveset);
		return PLH::FnCast(oTK__ApplyNewMoveset, TK__ApplyNewMoveset)(player, newMoveset);
	}
}

// -- Hooking --

void MovesetLoaderT7::PostInit()
{
	// TK__ApplyNewMoveset
	{
		m_hooks.push_back({
			.detour = nullptr,
			.name = "TK__ApplyNewMoveset"
		});
		auto& hook = m_hooks[m_hooks.size() - 1];

		// Get the original function's address
		TK__ApplyNewMoveset = 0;

		// Hook the original function
		hook.detour = new PLH::x64Detour((uint64_t)TK__ApplyNewMoveset, (uint64_t)&T7Hooks::ApplyNewMoveset, &oTK__ApplyNewMoveset, m_disassembler);
		hook.detour->hook();
	}
}