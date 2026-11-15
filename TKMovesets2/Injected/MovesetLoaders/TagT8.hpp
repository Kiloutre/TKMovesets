#pragma once

#include <cstdint>
#include <string>

class MovesetLoader;
class GameAddressesWrapper;

// Tag (two teams of two) in retail Tekken 8 Practice, run from inside the game by the injected loader.
// Port of the RE project's tag.py / fourp.py (Frida + Python) to native hooks:
//   - entering Practice builds the battle with 4 round entries (scene constructor count := 4), the Practice dummy
//     becomes P3 (team A's partner) and loader side 3 loads P4 (team B's partner) a few seconds into the match;
//   - Ki Charge (the game's 1+2+3+4 binding, or X+Y+A+B on the pad) tags the pressing side's team: the partner runs
//     in from off-screen and takes over on arrival, or at once when the enemy is airborne (tag combo).
// Every address comes from game_addresses.ini (t8-retail_tag_*, refreshed by Tools > Addresses from the exe).
namespace TagT8
{
	// Resolves the addresses and checks every hooked/called function's bytes against its pattern. false + reason
	// if anything is missing or does not match this executable.
	bool Init(MovesetLoader* loader, uint64_t moduleBase, uint64_t moduleSize, const GameAddressesWrapper& addresses, std::string& error);
	bool IsInitialized();
	// p4 < 0: P2 fights alone (only team A tags). Installs the hooks on first use.
	bool Enable(int p3Fighter, int p4Fighter, std::string& error);
	// No Tag at the next Practice entry. A running Tag match keeps its guards until the match ends.
	// Refused (false) while P3/P4 are being added to the match
	bool Disable();
	// P3/P4 being added (grow pending or a slot setup running)
	bool Loading();
	// Removes the hooks when no thread is inside them (before the loader unloads)
	void Shutdown();
	// Request a tag for team 0 (A) or 1 (B), as Ki Charge does
	void RequestTagFor(int team);
	// Test input: masks played one per battle frame on the fighter in `slot`, then its moves are logged
	void PlayInput(int slot, const uint32_t* masks, int count);
	// TagState_ value and a readable line for the tool
	int State();
	std::string Status();
}
