#include <stdarg.h>

#include "MovesetLoader_t7.hpp"
#include "Helpers.hpp"

#include "steam_api.h"

using namespace StructsT7;

// Reference to the MovesetLoader
MovesetLoaderT7* g_loader = nullptr;

// -- Game functions -- //

namespace T7Functions
{
	// Called after every loading screen on each player address to write the moveset to their offsets
	typedef uint64_t(*ApplyNewMoveset)(void* player, MovesetInfo* newMoveset);

	// Returns the address of a player from his playerid (this is character-related stuff)
	typedef uint64_t(*GetPlayerFromID)(unsigned int playerId);

	typedef void(*ExecuteExtraprop)(void* player, Requirement* prop, __ida_int128 a3, char a4, char a5, float a6, __ida_int128 a7, __ida_int128 a8, __ida_int128 a9, __ida_int128 a10, __ida_int128 a11, uint64_t a12);
}

// -- Helpers --
// Try to cache variable addresses when possible instead of calling the helpers too much

static uint64_t* GetPlayerList()
{
	uint64_t baseFuncAddr = g_loader->GetFunctionAddr("TK__GetPlayerFromID");

	// Base ourselves on the instructions (lea rcx, ...) to find the address since values are relative form the function start
	uint32_t offset = *(uint32_t*)(baseFuncAddr + 11);
	uint64_t offsetBase = baseFuncAddr + 15;

	return (uint64_t*)(offsetBase + offset);
}

static unsigned int IsLocalPlayerP1()
{
	auto addr = g_loader->addresses.ReadPtrPathInCurrProcess("pid_addr", g_loader->moduleAddr);
	if (addr != 0) {
		return *(unsigned int*)addr == 0;
	}
	DEBUG_LOG("Failed to find p1\n");
	return true;
}

static int GetPlayerIdFromAddress(void* playerAddr)
{
	auto playerList = (uint64_t*)g_loader->variables["gTK_playerList"];
	for (int i = 0; i < 2; ++i) // The list technically goes to 6 but this game doesn't handle it so who cares
	{
		if (playerList[i] == (uint64_t)playerAddr) {
			return i;
		}
	}
	return -1;
}

static void ApplyStaticMotas(void* player, MovesetInfo* newMoveset)
{
	// This is required for some MOTAs to be properly applied (such as camera, maybe more)
	const uint64_t staticCameraOffset = g_loader->addresses.GetValue("static_camera_offset");
	MotaHeader** staticMotas = (MotaHeader**)((char*)player + staticCameraOffset);

	staticMotas[0] = newMoveset->motas.camera_1;
	staticMotas[1] = newMoveset->motas.camera_2;
}

// -- Other helpers -- //

static void InitializeCustomMoveset(SharedMemT7_Player& player)
{
	MovesetInfo* moveset = (MovesetInfo*)player.custom_moveset_addr;

	// Mark missing motas with bitflag
	for (unsigned int i = 0; i < _countof(moveset->motas.motas); ++i)
	{
		if ((uint64_t)moveset->motas.motas[i] == MOVESET_ADDR_MISSING) {
			player.SetMotaMissing(i);
		}
	}

	// .previous_character_id is used to determine which character id we will have to replace in requirements
	// Ensure it is at the right value for the first ApplyNewMoveset()
	player.previous_character_id = player.moveset_character_id;

	player.is_initialized = true;
}

static unsigned int GetLobbySelfMemberId()
{
	CSteamID lobbyID = CSteamID(g_loader->ReadVariable<uint64_t>("gTK_roomId_addr"));
	CSteamID mySteamID = SteamHelper::SteamUser()->GetSteamID();

	unsigned int lobbyMemberCount = SteamHelper::SteamMatchmaking()->GetNumLobbyMembers(lobbyID);
	for (unsigned int i = 0; i < lobbyMemberCount; ++i)
	{
		CSteamID memberId = SteamHelper::SteamMatchmaking()->GetLobbyMemberByIndex(lobbyID, i);
		if (memberId == mySteamID) {
			return i;
		}
	}
	return 0;
}

// -- Hook functions --

namespace T7Hooks
{
	uint64_t ApplyNewMoveset(void* player, MovesetInfo* newMoveset)
	{
		DEBUG_LOG("\n- ApplyNewMoveset on player %llx, moveset is %llx -\n", (uint64_t)player, (uint64_t)newMoveset);

		// First, call the original function to let the game initialize all the important values in their new moveset
		auto retVal = g_loader->CastTrampoline<T7Functions::ApplyNewMoveset>("TK__ApplyNewMoveset")(player, newMoveset);

		if (!g_loader->sharedMemPtr->locked_in) {
			return retVal;
		}

		// Determine if we, the user, are the main player or if we are p2
		bool isLocalP1 = IsLocalPlayerP1();

		// Determine the player index of the player argument, and correct it with isLocalP1
		auto playerIndex = GetPlayerIdFromAddress(player) ^ (!isLocalP1);
		DEBUG_LOG("Corrected player index: %d. Is Local P1: %d\n", playerIndex, isLocalP1);
		if (playerIndex < 0) {
			return retVal;
		}

		// Obtain custom player data to apply
		auto& playerData = g_loader->sharedMemPtr->players[playerIndex];
		MovesetInfo* customMoveset = (MovesetInfo*)playerData.custom_moveset_addr;
		if (!customMoveset) {
			return retVal;
		}
		DEBUG_LOG("Custom moveset %p\n", customMoveset);

		if (!playerData.is_initialized) {
			InitializeCustomMoveset(playerData);
		}

		// Copy missing MOTA offsets from newMoveset
		for (unsigned int i = 0; i < _countof(customMoveset->motas.motas); ++i)
		{
			if (playerData.isMotaMissing(i)) {
				DEBUG_LOG("Missing mota %d, getting it from game's moveset\n", i);
				customMoveset->motas.motas[i] = newMoveset->motas.motas[i];
			}
		}

		{
			// Fix moves relying on character IDs

			// Find out which character ID we will be replacing and with which
			int new_char_id = *(int*)((char*)player + 0xD8);
			int char_id_to_replace = playerData.previous_character_id;

			DEBUG_LOG("Char ID that we will be replacing: %d. Replacement: %d\n", char_id_to_replace, new_char_id);

			// Get the 'Is character ID' condition
			// todo: there are more than one condition that use character IDs. Might be relevant to also work on those?
			int c_characterIdCondition = (int)g_loader->addresses.GetValue("character_id_condition");

			// Loop through every requirement, replace character IDs in relevant requirements' values
			for (auto& requirement : StructIterator<Requirement>(customMoveset->table.requirement, customMoveset->table.requirementCount))
			{
				if (requirement.condition == c_characterIdCondition) {
					requirement.param_unsigned = (requirement.param_unsigned == char_id_to_replace) ? new_char_id : (new_char_id + 1);
				}
			}

			// Mark which character ID we will have to replace on the next ApplyNewMoveset() call
			playerData.previous_character_id = new_char_id;
		}

		
		{
			// Apply custom moveset to our character*
			DEBUG_LOG("Applying custom moveset to character...\n");
			auto addr = (char*)player + g_loader->addresses.GetValue("motbin_offset");
			auto motbinOffsetList = (MovesetInfo**)(addr);

			motbinOffsetList[0] = customMoveset;
			motbinOffsetList[1] = customMoveset;
			motbinOffsetList[2] = customMoveset;
			motbinOffsetList[3] = customMoveset;
			motbinOffsetList[4] = customMoveset;

			ApplyStaticMotas(player, customMoveset);
		}

		return retVal;
	}

	// Log function 
	void Log(const char* format, ...)
	{
		va_list argptr;
		va_start(argptr, format);
		vfprintf(stdout, format, argptr);
		va_end(argptr);
	}


	void ExecuteExtraprop(void* player, Requirement* prop, __ida_int128 a3, char a4, char a5, float a6, __ida_int128 a7, __ida_int128 a8, __ida_int128 a9, __ida_int128 a10, __ida_int128 a11, uint64_t a12)
	{

		if ((uint64_t)player == GetPlayerList()[0]) {

			DEBUG_LOG("TK__ExecuteExtraprop - PROP[%x|%u], %p, ", prop->condition, prop->param_unsigned, player);
			DEBUG_LOG("%u, %u, %.3f, ", a4, a5, a6);
			print_int128(a7);
			print_int128(a8);
			print_int128(a9);
			print_int128(a10);
			print_int128(a11);
			DEBUG_LOG("%llx\n", a12);
		}
		g_loader->CastTrampoline<T7Functions::ExecuteExtraprop>("TK__ExecuteExtraprop")(player, prop, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12);
	}
}

// -- -- //

void ExecuteInstantExtraprop(int playerid, uint32_t propId, uint32_t propValue)
{
	if (!g_loader->HasFunction("TK__ExecuteExtraprop")) {
		return;
	}

	DEBUG_LOG("ExecuteInstantExtraprop(%d, %x, %u)\n", playerid, propId, propValue);

	void* player = (void*)(GetPlayerList()[playerid]);
	Requirement prop = { .condition = propId, .param_unsigned = propValue };

	__ida_int128 a3 = { 0 };
	char a4;
	char a5;
	float a6;
	__ida_int128 a7 = { 0 };
	__ida_int128 a8 = { 0 };
	__ida_int128 a9 = { 0 };
	__ida_int128 a10 = { 0 };
	__ida_int128 a11 = { 0 };
	uint64_t a12 = { 0 };

	SET_INT128(a3, 0, 0x4000000000000000);
	a4 = 0;
	a5 = 0;
	a6 = 0;
	SET_INT128(a7, 0, 0x4000000000000000);
	SET_INT128(a8, 0, 0x4000000000000000);
	SET_INT128(a9, 1, 0x4000000000000000);
	SET_INT128(a10, 1, 0x4000000000000000);
	SET_INT128(a11, 0, 0x4000000000000000);
	a12 = 0;


	g_loader->CastFunction<T7Functions::ExecuteExtraprop>("TK__ExecuteExtraprop")(player, &prop, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12);
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
		//"TK__NetInterCS::MatchedAsClient"
	};

	// Crucial functions / hooks
	RegisterHook("TK__ApplyNewMoveset", m_moduleName, "f_ApplyNewMoveset", (uint64_t)&T7Hooks::ApplyNewMoveset);
	RegisterFunction("TK__GetPlayerFromID", m_moduleName, "f_GetPlayerFromID");
	//RegisterFunction("TK__NetInterCS::MatchedAsClient", m_moduleName, "f_NetInterCS");

	// Other less important things
	RegisterFunction("TK__ExecuteExtraprop", m_moduleName, "f_ExecuteExtraprop");

	variables["gTK_roomId_addr"] = addresses.ReadPtrPathInCurrProcess("steamLobbyId_addr", moduleAddr);

	{
#ifdef BUILD_TYPE_DEBUG
		// Also hook extraprop to debug print it
		RegisterHook("TK__ExecuteExtraprop", m_moduleName, "f_ExecuteExtraprop", (uint64_t)&T7Hooks::ExecuteExtraprop);

		// Find TK__Log
		auto funcAddr = addresses.ReadPtrPathInCurrProcess("f_Log_addr", moduleAddr);
		if (InjectionUtils::compare_memory_string((void*)funcAddr, addresses.GetString("f_Log")))
		{
			InitHook("TK__Log", funcAddr, (uint64_t)&T7Hooks::Log);
			DEBUG_LOG("Found Log function\n");
		}
#endif
	}
}
void MovesetLoaderT7::PostInit()
{
	sharedMemPtr = (SharedMemT7*)orig_sharedMemPtr;

	// Compute important variable addresses
	variables["gTK_playerList"] = (uint64_t)GetPlayerList();

	// Apply the hooks that need to be applied immediately
	HookFunction("TK__ApplyNewMoveset");
	//HookFunction("TK__ExecuteExtraprop");

	// Other
	HookFunction("TK__Log");
}

// -- Debug -- //

void MovesetLoaderT7::Debug()
{
#ifdef BUILD_TYPE_DEBUG
	CSteamID lobbyID = CSteamID(ReadVariable<uint64_t>("gTK_roomId_addr"));
	CSteamID mySteamID = SteamHelper::SteamUser()->GetSteamID();

	if (lobbyID == k_steamIDNil) {
		printf("Invalid lobby ID\n");
	}
	
	unsigned int lobbyMemberCount = SteamHelper::SteamMatchmaking()->GetNumLobbyMembers(lobbyID);
	printf("Lobby member count = %u\n", lobbyMemberCount);
	for (unsigned int i = 0; i < lobbyMemberCount; ++i)
	{
		printf("4\n");
		CSteamID memberId = SteamHelper::SteamMatchmaking()->GetLobbyMemberByIndex(lobbyID, i);
		printf("5\n");
		const char* name = SteamHelper::SteamFriends()->GetFriendPersonaName(memberId);
		printf("6\n");
		printf("%u. [%s] - self = %d\n", i, name, memberId == mySteamID);
	}
#endif
}

// -- Main -- //

void MovesetLoaderT7::Mainloop()
{
	while (!mustStop)
	{



		std::this_thread::sleep_for(std::chrono::milliseconds(GAME_INTERACTION_THREAD_SLEEP_MS));
	}
}

// -- Interaction -- //

void MovesetLoaderT7::ExecuteExtraprop()
{
	auto& propData = sharedMemPtr->propToPlay;
	ExecuteInstantExtraprop(propData.playerid, propData.id, propData.value);
}