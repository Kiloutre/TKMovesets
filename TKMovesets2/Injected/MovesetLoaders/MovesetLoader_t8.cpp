#include <stdarg.h>

#include "MovesetLoader_t8.hpp"
#include "Helpers.hpp"
#include "steamHelper.hpp"

#include "steam_api.h"

using namespace StructsT8;

// Reference to the MovesetLoader
MovesetLoaderT8* g_loader = nullptr;

# define GET_HOOK(x) ((uint64_t)&T8Hooks::x)

// -- Game functions -- //

namespace T8Functions
{
	typedef void**(*Log)(void** a1, const char* a2, size_t a3, int64 a4);
}

// -- Helpers --
// Try to cache variable addresses when possible instead of calling the helpers too much


// -- Other helpers -- //


// -- Hook functions --

namespace T8Hooks
{
	// Log function 
	void** Log(void** a1, const char* format, size_t a3, int64 a4)
	{
		printf("%s\n", format);
		return g_loader->CastTrampoline<T8Functions::Log>("TK__Log")(a1, format, a3, a4);
	}

}

// -- -- //

// -- Hooking init --

void MovesetLoaderT8::InitHooks()
{
	// Set the global reference to us
	g_loader = this;

	// Declare which hooks must absolutely be found to initialize this DLL
	m_requiredSymbols = {
		// Hooks
		// Functions
	};

	// Crucial functions / hooks

	// Other less important things

	{
#ifdef BUILD_TYPE_DEBUG
		// Find TK__Log
		RegisterHook("TK__Log", m_moduleName, "f_Log", GET_HOOK(Log));
#endif
	}
}
void MovesetLoaderT8::OnInitEnd()
{
	sharedMemPtr = (SharedMemT8*)orig_sharedMemPtr;

	// Compute important variable addresses

	// Apply the hooks that need to be applied immediately

	// Other
	HookFunction("TK__Log");
}

// -- Main -- //

void MovesetLoaderT8::Mainloop()
{
	DEBUG_LOG("Mainloop()\n");
	while (!mustStop)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(GAME_INTERACTION_THREAD_SLEEP_MS));
	}
}

// -- Interaction -- //

// -- Debug -- //

void MovesetLoaderT8::Debug()
{
#ifdef BUILD_TYPE_DEBUG

#endif
}