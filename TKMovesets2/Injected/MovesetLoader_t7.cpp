#include "MovesetLoader_t7.hpp"

#include "Helpers.hpp"

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

// -- Hook functions --

namespace T7Hooks
{
	uint64_t ApplyNewMoveset(void* player, MovesetInfo* newMoveset)
	{
		DEBUG_LOG("ApplyNewMoveset on player %llx, moveset is %llx\n", (uint64_t)player, (uint64_t)newMoveset);

		auto retVal = g_loader->CastTrampoline<T7Functions::ApplyNewMoveset>("TK__ApplyNewMoveset")(player, newMoveset);

		if (!g_loader->sharedMemPtr->locked_in) {
			//return retVal;
		}

		// Determine if we, the user, are the main player or if we are p2
		bool isLocalP1 = g_loader->ReadVariable<unsigned int>("gTK_playerid") == 0;

		// Determine the player index of the player argument, and correct it with isLocalP1
		auto playerIndex = GetPlayerIdFromAddress(player) ^ (!isLocalP1);
		DEBUG_LOG("Corrected player index: %d. Is Local P1: %d\n", playerIndex, isLocalP1);
		if (playerIndex == -1) {
			return retVal;
		}

		// Obtain custom player data to apply
		auto& playerData = g_loader->sharedMemPtr->players[playerIndex];
		MovesetInfo* customMoveset = (MovesetInfo*)playerData.custom_moveset_addr;
		if (!customMoveset) {
			return retVal;
		}
		DEBUG_LOG("Custom moveset %llx\n", customMoveset);

		// Copy missing MOTA offsets from newMoveset
		for (unsigned int i = 0; i < _countof(customMoveset->motas.motas); ++i)
		{
			if (playerData.isMotaMissing(i)) {
				DEBUG_LOG("Missing mota %d, getting it from game's moveset\n", i);
				customMoveset->motas.motas[i] = newMoveset->motas.motas[i];
			}
		}

					
		/*
		// Fix moves relying on character IDs
		int currentCharId = *(int*)((char*)player + 0xD8);
		int previousCharacterId = playerData.previous_character_id;
		if (previousCharacterId == SHARED_MEM_MOVESET_NO_CHAR) {
			previousCharacterId = playerData.moveset_character_id;
		}

		int c_characterIdCondition = (int)g_loader->addresses.GetValue("character_id_condition");

		for (auto& requirement : StructIterator<Requirement>(customMoveset->table.requirement, customMoveset->table.requirementCount))
		{
			if (requirement.condition == c_characterIdCondition) {
				requirement.param_unsigned = (requirement.param_unsigned == previousCharacterId) ? currentCharId : currentPlayerId + 1;
			}
		}

		*/

		
		// Apply new moveset to our character*
		DEBUG_LOG("Applying new moveset to character...\n");
		auto addr = (char*)player + g_loader->addresses.GetValue("motbin_offset");
		auto motbinOffsetList = (MovesetInfo**)(addr);

		motbinOffsetList[0] = customMoveset;
		motbinOffsetList[1] = customMoveset;
		motbinOffsetList[2] = customMoveset;
		motbinOffsetList[3] = customMoveset;
		motbinOffsetList[4] = customMoveset;

		return retVal;
	}
}

// -- Other -- //

static void InitializeMoveset(SharedMemT7_Player& player)
{
	MovesetInfo* moveset = (MovesetInfo*)player.custom_moveset_addr;

	// Mark missing motas with bitflag
	for (unsigned int i = 0; i < _countof(moveset->motas.motas); ++i)
	{
		if ((uint64_t)moveset->motas.motas[i] == MOVESET_ADDR_MISSING) {
			player.SetMotaMissing(i);
		}
	}

	//player.previous_character_id = player.moveset_character_id;
	player.is_initialized = true;
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
	RegisterHook("TK__ApplyNewMoveset", m_moduleName, "f_ApplyNewMoveset", (uint64_t)&T7Hooks::ApplyNewMoveset);
	RegisterFunction("TK__GetPlayerFromID", m_moduleName, "f_GetPlayerFromID");
	RegisterFunction("TK__GetTK3447820", m_moduleName, "f_GetTK3447820");
}

void MovesetLoaderT7::PostInit()
{
	sharedMemPtr = (SharedMemT7*)orig_sharedMemPtr;

	// Compute important variable addresses
	variables["gTK_playerList"] = (uint64_t)GetPlayerList();

	{
		// There is an actual player side global variable outside of any structure
		// That isn't it, but this function allows us to get the same value in a more reliable way
		auto structAddr = g_loader->CastFunction<T7Functions::GetTK3447820>("TK__GetTK3447820")();
		variables["gTK_playerid"] = structAddr + 0x6C;
	}

	// Apply the hooks that need to be applied immediately
	m_hooks["TK__ApplyNewMoveset"].detour->hook();

	for (unsigned int i = 0; i < _countof(sharedMemPtr->players); ++i) {
		sharedMemPtr->players[i].previous_character_id = SHARED_MEM_MOVESET_NO_CHAR;
	}
}

// -- Main -- //

void MovesetLoaderT7::Mainloop()
{
	while (!mustStop)
	{
		auto& players = sharedMemPtr->players;
		for (int i = 0; i < 2; ++i)
		{
			if (!players[i].is_initialized && players[i].custom_moveset_addr != 0) {
				DEBUG_LOG("New moveset loaded, initializing moveset of player [%d] in the array\n", i);
				InitializeMoveset(players[i]);
			}
		}
		//
		std::this_thread::sleep_for(std::chrono::milliseconds(GAME_INTERACTION_THREAD_SLEEP_MS));
	}
}