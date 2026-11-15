#pragma once

#include <cstdint>
#include <string>

class GameAddressesWrapper;

// Grows a RUNNING retail Tekken 8 battle from 3 (or fewer) player slots to 4 (or up to kMaxTarget), in place: no reload, no loading screen.
// Used by the Tag mode (TagT8.cpp) to add P4 to a Practice match that was built with 3 slots.
//
// How (RE repo docs/tag-retail.md "Growing a running battle to 4 slots"):
// the battle core (registry global, scene+0x48) is {vt, int count at +8, registry object at +0x10}. The registry
// object holds 30 services at fixed slots (+8*k); 11 of them size std::vectors from the count. For each of those the
// routine builds a throwaway copy with count 4-n through the game's own constructor, then gives every count-sized
// vector of the live service the old n elements followed by the fresh ones (copied, not moved: old buffers and the
// throwaway shells are leaked on purpose, so anything that still points at them reads valid memory). The round keeps
// its entries; the fresh round entries (2 per slot) come from the round constructor, exactly as a 4-count battle
// leaves an unused slot: constructed, not in the round's slot or iteration vectors (inactive).
// After the copy: the core service's per-element nodes are pointed back at the elements' new addresses, battle memory
// (the 11 services, the round and its entries) is scanned for pointers into the old buffers and relocated, and the
// core count becomes 4.
//
// Threading: call on the game thread, between two battle frames (end of the frame hook). Nothing else may run battle
// code during the call. It does not activate slot 3: the Tag code then runs its usual P4 steps on entry 3.
namespace TagT8Grow
{
	// Count-sized services, by registry slot. kRound is slot 6: the slot holds an 8-byte holder whose qword is the
	// round object (0x88 bytes); its ctor is the round's own, not the holder's.
	enum Service : int {
		kS04, kRound, kS09, kS0A, kS14, kS16, kS17, kS18, kS19, kS1B, kS1C,
		kServiceCount
	};
	// game_addresses.ini keys of the ctors, without the "t8-retail_" prefix, in Service order
	extern const char* const kCtorKeys[kServiceCount];

	struct GrowAddresses {
		uint64_t malloc = 0;                  // t8-retail_tag_malloc: operator new(size) -> FMemory::Malloc(size, 16)
		uint64_t registry = 0;                // t8-retail_tag_registry: global holding the running battle core
		uint64_t ctor[kServiceCount] = {};    // t8-retail_tag_grow_ctor_sXX: (obj, int* count) -> obj
	};

	// Where the pointer scan found a pointer into an old buffer (for the log)
	struct PointerHit {
		uint8_t region = 0;       // 0 service object, 1 vector buffer, 2 round slot table, 3 round entry
		uint8_t regionSlot = 0;   // registry slot of the service (0 and 1), 6 for the round (2 and 3)
		uint16_t regionOff = 0;   // 1: the vector's offset in its service; 2/3: round +0x8/+0x50 table, +0x38/+0x68 vector
		uint8_t entry = 0;        // 3: entry index
		uint32_t at = 0;          // byte offset of the qword in the region
		uint8_t vecSlot = 0;      // the old buffer it pointed into: service slot + vector offset
		uint16_t vecOff = 0;
		uint32_t into = 0;        // offset of the target in the old buffer (== old size for an end pointer)
	};
	static constexpr int kMaxHits = 8;

	struct GrowReport {
		int oldCount = 0;             // core count before the call
		int newCount = 0;             // after (== oldCount: nothing done)
		int vectorsReallocated = 0;   // vectors that got a new 4-element buffer
		int vectorsInPlace = 0;       // vectors grown inside their capacity (14195EE00 +0x20: reserved 0x400; bit vectors)
		int pointersRelocated = 0;    // qwords in battle memory that pointed into an old buffer, rewritten
		int staleEndPointers = 0;     // qwords equal to an old buffer's end (not rewritten: ambiguous); should be 0
		uint64_t bytesLeaked = 0;     // old buffers + throwaway service shells
		PointerHit relocated[kMaxHits]; int relocatedHits = 0;   // the first kMaxHits of each
		PointerHit stale[kMaxHits]; int staleHits = 0;
	};

	// Reads the t8-retail_tag_malloc / tag_registry / tag_grow_ctor_* keys and checks each ctor's bytes against its
	// sig_*_1 [f] pattern. false + reason when a key is missing or a pattern does not match this executable.
	bool ResolveAddresses(uint64_t moduleBase, uint64_t moduleSize, const GameAddressesWrapper& addresses,
		GrowAddresses& out, std::string& error);

	// Largest target count. The grow itself is count-generic (bit vectors hold 32); what limits it is the game code that
	// indexes players: see RE repo docs/tag-retail.md "More than 4 slots".
	constexpr int kMaxTarget = 6;

	// The running battle (core = *registry) to `target` slots (2..kMaxTarget). true: the battle has at least `target`
	// slots now (already had: report->newCount == oldCount, nothing done). false + reason: nothing was changed, except
	// when the reason starts with "FAULT during commit" (then the battle may be broken).
	bool GrowBattleTo(const GrowAddresses& a, int target, std::string& error, GrowReport* report = nullptr);
	// Same on a given battle core
	bool GrowCoreTo(const GrowAddresses& a, uint64_t core, int target, std::string& error, GrowReport* report = nullptr);
	// target 4
	bool GrowBattleTo4(const GrowAddresses& a, std::string& error, GrowReport* report = nullptr);
	bool GrowCoreTo4(const GrowAddresses& a, uint64_t core, std::string& error, GrowReport* report = nullptr);

	// One log line for a report (the pointer hits after it, "slot+off[k]" notation)
	std::string Describe(const GrowReport& r);
}
