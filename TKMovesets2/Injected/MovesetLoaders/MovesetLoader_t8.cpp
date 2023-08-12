#include <stdarg.h>
#include <fstream>
#include <string>
#include <format>

#include "MovesetLoader_t8.hpp"
#include "Helpers.hpp"
#include "steamHelper.hpp"

#include "steam_api.h"

using namespace StructsT8;

// Reference to the MovesetLoader
MovesetLoaderT8* g_loader = nullptr;
std::ofstream* g_log = nullptr;
unsigned int g_roundType = 0;

# define GET_HOOK(x) ((uint64_t)&T8Hooks::x)

// -- Game functions -- //

namespace T8Functions
{
	typedef void** (*Log)(void** a1, const char* a2, size_t a3, int64 a4);

	typedef char (*ResetRound)(unsigned int a1);
}

// -- Helpers --
// Try to cache variable addresses when possible instead of calling the helpers too much


// -- Other helpers -- //


// -- Hook functions --

namespace T8Hooks
{
	// Log function 
	void** Log(void** a1, const char* format, size_t string_length, int64 a4)
	{
		printf("%s\n", format);
		if (g_log) {
			g_log->write("[", 1);
			std::string t = Helpers::formatDateTime(Helpers::getCurrentTimestamp(), false);
			g_log->write(t.c_str(), t.size());
			g_log->write("] ", 2);
			g_log->write(format, string_length);
			g_log->write("\n", 1);
		}
		return g_loader->CastTrampoline<T8Functions::Log>("TK__Log")(a1, format, string_length, a4);
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
		"TK__ResetRound"
	};

	// Crucial functions / hooks

	// Other less important things

#ifdef BUILD_TYPE_DEBUG
	// Find TK__Log
	RegisterHook("TK__Log", m_moduleName, "f_Log", GET_HOOK(Log));
#endif sk
	RegisterFunction("TK__ResetRound", m_moduleName, "f_ResetRound");

}
void MovesetLoaderT8::OnInitEnd()
{
	sharedMemPtr = (SharedMemT8*)orig_sharedMemPtr;

	// Compute important variable addresses

	// Apply the hooks that need to be applied immediately

	// Other
#ifdef BUILD_TYPE_DEBUG
	HookFunction("TK__Log");
#endif
	
	DEBUG_LOG("g_roundType = %p\n", &g_roundType);


	std::string filename = std::format("G:/Logs_{}.txt", Helpers::formatDateTime(Helpers::getCurrentTimestamp(), true));
	g_log = new std::ofstream(filename);
}

// -- Main -- //

void MovesetLoaderT8::Mainloop()
{
	DEBUG_LOG("Mainloop()\n");
	while (!mustStop)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(GAME_INTERACTION_THREAD_SLEEP_MS));
	}
	g_log->close();
	DEBUG_LOG("Mainloop - END\n");
}

// -- Interaction -- //

// -- Debug -- //

void MovesetLoaderT8::Debug()
{
#ifdef BUILD_TYPE_DEBUG
	void* func_addr;
	switch (g_roundType)
	{
	case 0:
		func_addr = (void*)0x14153DE80;
		break;
	case 1:
		func_addr = (void*)0x14153DF20;
		break;
	default:
		return;
	}
	void (*test)(void) = (void(*)(void))func_addr;
	test();
	//g_loader->CastFunction<T8Functions::ResetRound>("TK__ResetRound")(g_roundType);
#endif
}