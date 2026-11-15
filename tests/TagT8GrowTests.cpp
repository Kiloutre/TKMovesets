// Offline test of TagT8Grow (TKMovesets2/Injected/MovesetLoaders/TagT8Grow.cpp): a fake retail T8 battle built in
// this process with the layouts of the game's ctors (RE repo work/retail/fourslot/svc_*.c, grow/reg/*.c), grown from
// 3 to 4 slots. No game, no injection.
//
// Checks: every count-sized vector ends with the 3 old elements (bytes kept, cross pointers relocated) then one fresh
// element exactly as the ctor builds it; allocator rule of the new buffers; node back pointers; bit vectors; round
// entries; pointer relocation (interior, self, from a round entry) and the stale end-pointer count; the throwaway
// objects let go of their buffers; no-op at 4; refusal (nothing changed) on a layout mismatch.

#include "TagT8Grow.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <malloc.h>
#include <vector>

using u64 = uint64_t;
using namespace TagT8Grow;

namespace
{
int g_failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { ++g_failures; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

template<class T> T& At(u64 a) { return *(T*)a; }

// ---- fake game allocator: operator new(size) -> 16-byte aligned (never freed: the grow never frees either) ------
u64 FakeMalloc(u64 size) { return (u64)_aligned_malloc(size ? size : 1, 16); }
// the vector buffer rule the game's ctors use
u64 GameVecAlloc(u64 bytes)
{
	if (bytes < 0x1000) return bytes ? FakeMalloc(bytes) : 0;
	const u64 raw = FakeMalloc(bytes + 0x27);
	const u64 al = (raw + 0x27) & ~(u64)0x1F;
	At<u64>(al - 8) = raw;
	return al;
}

// ---- service layouts (independent of TagT8Grow's own tables) -------------------------------------------------
enum VK { kV, kNode, kBits };
struct V { u64 off; u64 elem; VK kind; u64 reserve = 0; };
struct S { int slot; u64 size; u64 vtable; std::vector<V> vecs; };

std::vector<S> Services()
{
	std::vector<S> s(kServiceCount);
	s[kS04] = { 0x04, 0x1AA0, 0x1475BCE60, { { 0x378, 8, kV } } };
	s[kRound] = { 0x06, 0x88, 0x1475C0C68, { { 0x38, 8, kV }, { 0x68, 8, kV } } };
	s[kS09] = { 0x09, 0x160, 0, { { 0x0, 0x148, kV } } };
	s[kS0A] = { 0x0A, 0x18, 0, { { 0x0, 0x50, kV } } };
	s[kS14] = { 0x14, 0x58, 0x1475C17D8, { { 0x8, 0x2A0, kV }, { 0x20, 0x40, kV, 0x400 }, { 0x38, 8, kV } } };
	s[kS16] = { 0x16, 0x1E0, 0x1475BBC60, { { 0x08, 0x7B8, kV }, { 0x20, 0x12C0, kV } } };
	s[kS17] = { 0x17, 0x30, 0x1475BC268, { { 0x10, 8, kV } } };
	s[kS18] = { 0x18, 0x78, 0x1475C1638, { { 0x8, 4, kV }, { 0x20, 0, kBits }, { 0x40, 0, kBits } } };
	S h{ 0x19, 0xC38, 0x1475C1640, {} };
	for (u64 i = 0; i < 65; ++i) { h.vecs.push_back({ 0x8 + 0x30 * i, 8, kV }); h.vecs.push_back({ 0x20 + 0x30 * i, 4, kV }); }
	s[kS19] = h;
	s[kS1B] = { 0x1B, 0x40, 0x1475C0108, { { 0x10, 0x28, kNode }, { 0x28, 4, kV } } };
	S c{ 0x1C, 0x1A0, 0x1475C17E8, {} };
	for (u64 i = 0; i < 8; ++i) c.vecs.push_back({ 0x8 + 0x18 * i, 0xC0, kV });
	c.vecs.push_back({ 0xC8, 0x80, kV });
	for (u64 i = 0; i < 8; ++i) c.vecs.push_back({ 0xE0 + 0x18 * i, 0x80, kV });
	s[kS1C] = c;
	return s;
}
const std::vector<S> kS = Services();

constexpr u64 kEntrySize = 0x1F140;
constexpr u64 kFreshEntryMark = 0xFEEDFACE12345678ull;

// what a fresh element looks like (the same for every index, like the real ctors)
uint8_t FreshByte(int svc, u64 off, u64 j) { return (uint8_t)(0x31 + svc * 7 + off + j * 3); }

u64 NewEntry()
{
	const u64 e = FakeMalloc(kEntrySize);
	memset((void*)e, 0, kEntrySize);
	At<u64>(e) = kFreshEntryMark;
	return e;
}

u64 g_lastTmp[kServiceCount];
u64 g_vtableOverride[kServiceCount];   // != 0: the ctor writes this vtable (layout mismatch test)

void BuildService(int svc, u64 obj, int count)
{
	const S& d = kS[svc];
	memset((void*)obj, 0, d.size);
	if (d.vtable) At<u64>(obj) = g_vtableOverride[svc] ? g_vtableOverride[svc] : d.vtable;
	for (const V& v : d.vecs) {
		u64* hdr = (u64*)(obj + v.off);
		if (v.kind == kBits) {
			const u64 words = (count + 31) / 32;
			const u64 b = GameVecAlloc(words * 4);
			memset((void*)b, 0, words * 4);
			hdr[0] = b; hdr[1] = hdr[2] = b + words * 4; hdr[3] = (u64)count;
			continue;
		}
		const u64 cap = v.reserve > (u64)count ? v.reserve : (u64)count;
		const u64 b = GameVecAlloc(cap * v.elem);
		for (int k = 0; k < count; ++k) {
			const u64 el = b + k * v.elem;
			if (svc == kRound) { At<u64>(el) = NewEntry(); continue; }
			for (u64 j = 0; j < v.elem; ++j) At<uint8_t>(el + j) = FreshByte(svc, v.off, j);
			if (v.kind == kNode) { const u64 node = FakeMalloc(0x10); At<u64>(node) = el; At<u64>(node + 8) = 0; At<u64>(el) = node; }
		}
		hdr[0] = b; hdr[1] = b + count * v.elem; hdr[2] = b + cap * v.elem;
	}
	if (svc == kRound)
		for (u64 o : { 0x8ull, 0x50ull }) {
			const u64 t = FakeMalloc(0x200); memset((void*)t, 0, 0x200);
			At<u64>(obj + o) = t; At<u64>(obj + o + 8) = At<u64>(obj + o + 0x10) = t + 0x200;
		}
}

template<int I> u64 FakeCtor(u64 obj, int* count) { BuildService(I, obj, *count); g_lastTmp[I] = obj; return obj; }
using Ctor = u64(*)(u64, int*);
const Ctor kCtors[kServiceCount] = { FakeCtor<0>, FakeCtor<1>, FakeCtor<2>, FakeCtor<3>, FakeCtor<4>, FakeCtor<5>,
	FakeCtor<6>, FakeCtor<7>, FakeCtor<8>, FakeCtor<9>, FakeCtor<10> };

struct Battle { u64 core, reg; u64 obj[kServiceCount]; };

// battle core {vt, count, registry}; registry 0x138 with 30 slots, slot 6 = 8-byte holder -> round
Battle BuildBattle(int count)
{
	Battle bt{};
	bt.core = FakeMalloc(0x18);
	bt.reg = FakeMalloc(0x138);
	memset((void*)bt.reg, 0, 0x138);
	At<u64>(bt.core) = 0x1475C00D0; At<int32_t>(bt.core + 8) = count; At<u64>(bt.core + 0x10) = bt.reg;
	At<u64>(bt.reg) = 0x1475C0148;
	for (int k = 1; k <= 0x1E; ++k) At<u64>(bt.reg + 8 * k) = FakeMalloc(0x10);   // the other services: present, untouched
	for (int s = 0; s < kServiceCount; ++s) {
		const u64 o = FakeMalloc(kS[s].size);
		BuildService(s, o, count);
		bt.obj[s] = o;
		if (s == kRound) { const u64 holder = FakeMalloc(8); At<u64>(holder) = o; At<u64>(bt.reg + 0x30) = holder; }
		else At<u64>(bt.reg + 8ull * kS[s].slot) = o;
	}
	return bt;
}

u64 Mix(u64& x) { x += 0x9E3779B97F4A7C15ull; u64 z = x; z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull; z = (z ^ (z >> 27)) * 0x94D049BB133111EBull; return z ^ (z >> 31); }

// fills slots 0..n-1 with battle data (keeps the node qword of the core service's elements)
void PlayFrames(Battle& bt, int n)
{
	u64 seed = 1234;
	for (int s = 0; s < kServiceCount; ++s) {
		if (s == kRound) continue;
		for (const V& v : kS[s].vecs) {
			const u64 b = At<u64>(bt.obj[s] + v.off);
			if (v.kind == kBits) { At<uint32_t>(b) = 0b101; continue; }
			for (u64 q = 0; q + 8 <= n * v.elem; q += 8) {
				if (v.kind == kNode && q % v.elem == 0) continue;
				At<u64>(b + q) = Mix(seed);
			}
			for (u64 q = n * v.elem / 8 * 8; q < n * v.elem; ++q) At<uint8_t>(b + q) = (uint8_t)Mix(seed);
		}
	}
	for (u64 o : { 0x38ull, 0x68ull })
		for (int k = 0; k < n; ++k) At<u64>(At<u64>(At<u64>(bt.obj[kRound] + o) + 8 * k)) = 0x1111ull * (k + 1) + o;
}

struct Snap { int svc; u64 off, elem; std::vector<uint8_t> bytes; };
std::vector<Snap> Snapshot(const Battle& bt, int n)
{
	std::vector<Snap> out;
	for (int s = 0; s < kServiceCount; ++s)
		for (const V& v : kS[s].vecs) {
			if (v.kind == kBits) continue;
			const u64 b = At<u64>(bt.obj[s] + v.off);
			Snap sn{ s, v.off, v.elem, std::vector<uint8_t>(n * v.elem) };
			memcpy(sn.bytes.data(), (void*)b, n * v.elem);
			out.push_back(std::move(sn));
		}
	return out;
}
u64 VecB(const Battle& bt, int s, u64 off) { return At<u64>(bt.obj[s] + off); }
u64 VecE(const Battle& bt, int s, u64 off) { return At<u64>(bt.obj[s] + off + 8); }
u64 VecC(const Battle& bt, int s, u64 off) { return At<u64>(bt.obj[s] + off + 0x10); }

GrowAddresses Addresses(u64* registryGlobal)
{
	GrowAddresses a;
	a.malloc = (u64)&FakeMalloc;
	a.registry = (u64)registryGlobal;
	for (int s = 0; s < kServiceCount; ++s) a.ctor[s] = (u64)kCtors[s];
	return a;
}

void TestGrow()
{
	Battle bt = BuildBattle(3);
	PlayFrames(bt, 3);
	// cross pointers the grow must fix:
	//  s04 vector element 1 -> inside s09 element 2 (interior pointer into another moved buffer)
	const u64 s09b = VecB(bt, kS09, 0), s16b = VecB(bt, kS16, 0x20), s0aE = VecE(bt, kS0A, 0);
	At<u64>(VecB(bt, kS04, 0x378) + 8) = s09b + 2 * 0x148 + 0x10;
	//  s09 element 0 +0x20 -> s09 element 0 +0x40 (self pointer, moves with its buffer)
	At<u64>(s09b + 0x20) = s09b + 0x40;
	//  round entry 0 of +0x38, +0x1000 -> s16 element 1 (from an entry, which does not move)
	const u64 entry0 = At<u64>(VecB(bt, kRound, 0x38));
	At<u64>(entry0 + 0x1000) = s16b + 0x12C0;
	//  a service object field -> s17 element 2 (a cursor kept in the service)
	const u64 s17b = VecB(bt, kS17, 0x10);
	At<u64>(bt.obj[kS04] + 0x100) = s17b + 16;     // a zero field of the fake s04 object
	//  round entry 1 +0x2000 = s0a's old end (ambiguous: counted, not rewritten)
	const u64 entry1 = At<u64>(VecB(bt, kRound, 0x38) + 8);
	At<u64>(entry1 + 0x2000) = s0aE;

	const std::vector<Snap> before = Snapshot(bt, 3);
	u64 oldEntries[2][3];
	for (int i = 0; i < 2; ++i) for (int k = 0; k < 3; ++k) oldEntries[i][k] = At<u64>(VecB(bt, kRound, i ? 0x68 : 0x38) + 8 * k);
	u64 oldNodes[3];
	for (int k = 0; k < 3; ++k) oldNodes[k] = At<u64>(VecB(bt, kS1B, 0x10) + k * 0x28);
	const u64 s14InPlaceB = VecB(bt, kS14, 0x20);

	u64 registry = bt.core;
	const GrowAddresses a = Addresses(&registry);
	std::string err;
	GrowReport rep;
	const bool ok = GrowBattleTo4(a, err, &rep);
	CHECK(ok, "grow failed: %s", err.c_str());
	if (!ok) return;
	printf("%s\n", Describe(rep).c_str());
	CHECK(At<int32_t>(bt.core + 8) == 4, "core count %d", At<int32_t>(bt.core + 8));

	// every vector: old bytes, then the fresh element
	int plain = 0;
	for (const Snap& sn : before) {
		++plain;
		const u64 b = VecB(bt, sn.svc, sn.off), e = VecE(bt, sn.svc, sn.off), c = VecC(bt, sn.svc, sn.off);
		CHECK(e - b == 4 * sn.elem && c >= e, "svc %d +0x%llX: %llu bytes", sn.svc, sn.off, e - b);
		std::vector<uint8_t> expect = sn.bytes;
		auto put = [&](u64 at, u64 v) { memcpy(expect.data() + at, &v, 8); };
		if (sn.svc == kS04) put(8, VecB(bt, kS09, 0) + 2 * 0x148 + 0x10);
		if (sn.svc == kS09) put(0x20, b + 0x40);
		CHECK(!memcmp((void*)b, expect.data(), expect.size()), "svc %d +0x%llX: old elements changed", sn.svc, sn.off);
		const u64 el = b + 3 * sn.elem;
		if (sn.svc == kRound) {
			CHECK(At<u64>(At<u64>(el)) == kFreshEntryMark, "round +0x%llX: entry 3 is not a fresh entry", sn.off);
			continue;
		}
		bool fresh = true;
		for (u64 j = (sn.svc == kS1B && sn.off == 0x10) ? 8 : 0; j < sn.elem; ++j)
			if (At<uint8_t>(el + j) != FreshByte(sn.svc, sn.off, j)) { fresh = false; break; }
		CHECK(fresh, "svc %d +0x%llX: element 3 is not a fresh element", sn.svc, sn.off);
		// allocator rule of the game: big buffers 32-aligned with the raw pointer at [-8]
		if (4 * sn.elem >= 0x1000 && b != s14InPlaceB) {
			const u64 raw = At<u64>(b - 8);
			CHECK(b % 32 == 0 && b - raw >= 8 && b - raw <= 0x27, "svc %d +0x%llX: big buffer header", sn.svc, sn.off);
		}
		// the throwaway let go of it
		if (g_lastTmp[sn.svc]) CHECK(At<u64>(g_lastTmp[sn.svc] + sn.off) == 0, "svc %d +0x%llX: throwaway still owns its buffer", sn.svc, sn.off);
	}
	CHECK(VecB(bt, kS14, 0x20) == s14InPlaceB, "s14 +0x20 (reserved 0x400) was reallocated");
	CHECK(VecC(bt, kS14, 0x20) - VecB(bt, kS14, 0x20) == 0x400 * 0x40, "s14 +0x20 capacity changed");

	// cross pointers
	CHECK(At<u64>(At<u64>(VecB(bt, kRound, 0x38)) + 0x1000) == VecB(bt, kS16, 0x20) + 0x12C0, "entry -> s16 pointer not relocated");
	CHECK(At<u64>(bt.obj[kS04] + 0x100) == VecB(bt, kS17, 0x10) + 16, "service cursor -> s17 not relocated");
	CHECK(At<u64>(entry1 + 0x2000) == s0aE, "stale end pointer was rewritten");
	CHECK(rep.staleEndPointers == 1, "stale end pointers %d", rep.staleEndPointers);
	CHECK(rep.staleHits == 1 && rep.stale[0].region == 3 && rep.stale[0].regionOff == 0x38 && rep.stale[0].entry == 1 &&
		rep.stale[0].at == 0x2000 && rep.stale[0].vecSlot == 0x0A && rep.stale[0].vecOff == 0 && rep.stale[0].into == 3 * 0x50,
		"stale hit location not recorded right");
	CHECK(rep.relocatedHits == 4, "relocated hits %d", rep.relocatedHits);
	CHECK(rep.pointersRelocated == 4, "pointers relocated %d (expected 4)", rep.pointersRelocated);
	CHECK(rep.vectorsInPlace == 3 && rep.vectorsReallocated == plain - 1, "reallocated %d in place %d", rep.vectorsReallocated, rep.vectorsInPlace);

	// round entries: same old entries, fresh 4th
	for (int i = 0; i < 2; ++i) for (int k = 0; k < 3; ++k)
		CHECK(At<u64>(VecB(bt, kRound, i ? 0x68 : 0x38) + 8 * k) == oldEntries[i][k], "round entry %d/%d moved", i, k);
	// core service nodes: same nodes, pointing at the new elements; the fresh one too
	const u64 nb = VecB(bt, kS1B, 0x10);
	for (int k = 0; k < 4; ++k) {
		const u64 el = nb + k * 0x28, node = At<u64>(el);
		CHECK(node && At<u64>(node) == el, "node %d does not point back", k);
		if (k < 3) CHECK(node == oldNodes[k], "node %d replaced", k);
	}
	// bit vectors: 4 bits, old bits kept, new bit clear
	for (u64 o : { 0x20ull, 0x40ull }) {
		const u64 hdr = bt.obj[kS18] + o;
		CHECK(At<u64>(hdr + 0x18) == 4, "bits +0x%llX: size %llu", o, At<u64>(hdr + 0x18));
		CHECK((At<uint32_t>(At<u64>(hdr)) & 0xF) == 0b101, "bits +0x%llX: word %x", o, At<uint32_t>(At<u64>(hdr)));
	}

	// second call: nothing to do
	GrowReport rep2;
	const u64 b04 = VecB(bt, kS04, 0x378);
	CHECK(GrowBattleTo4(a, err, &rep2) && rep2.oldCount == 4 && rep2.newCount == 4 && VecB(bt, kS04, 0x378) == b04, "second call changed something");
}

// a layout mismatch: refused, nothing live changed
void TestRefusals()
{
	{
		Battle bt = BuildBattle(3);
		At<u64>(bt.obj[kS17] + 0x18) -= 8;          // s17 vector has 2 elements: something else sized it
		const std::vector<Snap> before = Snapshot(bt, 2);
		u64 registry = bt.core;
		std::string err;
		CHECK(!GrowBattleTo4(Addresses(&registry), err, nullptr), "grew a battle with a 2-element vector");
		printf("refused (expected): %s\n", err.c_str());
		CHECK(At<int32_t>(bt.core + 8) == 3, "count changed on refusal");
		for (const Snap& sn : before) CHECK(!memcmp((void*)VecB(bt, sn.svc, sn.off), sn.bytes.data(), sn.bytes.size()), "svc %d changed on refusal", sn.svc);
	}
	{
		Battle bt = BuildBattle(3);
		g_vtableOverride[kS1C] = 0x1400000000ull;  // the ctor at that address builds something else
		const u64 b = VecB(bt, kS1C, 0xC8);
		u64 registry = bt.core;
		std::string err;
		CHECK(!GrowBattleTo4(Addresses(&registry), err, nullptr), "grew with a ctor of another class");
		printf("refused (expected): %s\n", err.c_str());
		CHECK(At<int32_t>(bt.core + 8) == 3 && VecB(bt, kS1C, 0xC8) == b, "changed on refusal");
		g_vtableOverride[kS1C] = 0;
	}
	{
		Battle bt = BuildBattle(3);
		const u64 el = VecB(bt, kS1B, 0x10) + 0x28;
		At<u64>(At<u64>(el)) = 0;                   // node no longer points back: not the layout we know
		u64 registry = bt.core;
		std::string err;
		CHECK(!GrowBattleTo4(Addresses(&registry), err, nullptr), "grew with a broken node");
		printf("refused (expected): %s\n", err.c_str());
	}
	{
		std::string err;
		u64 registry = 0;
		CHECK(!GrowBattleTo4(Addresses(&registry), err, nullptr), "grew without a battle");
	}
}

// 1 and 2 slots grow too (count-generic)
void TestSmallCounts()
{
	for (int n : { 1, 2 }) {
		Battle bt = BuildBattle(n);
		PlayFrames(bt, n);
		const std::vector<Snap> before = Snapshot(bt, n);
		u64 registry = bt.core;
		std::string err;
		const bool ok = GrowBattleTo4(Addresses(&registry), err, nullptr);
		CHECK(ok, "n=%d: %s", n, err.c_str());
		if (!ok) continue;
		for (const Snap& sn : before) {
			const u64 b = VecB(bt, sn.svc, sn.off);
			CHECK(VecE(bt, sn.svc, sn.off) - b == 4 * sn.elem, "n=%d svc %d size", n, sn.svc);
			CHECK(!memcmp((void*)b, sn.bytes.data(), sn.bytes.size()), "n=%d svc %d old changed", n, sn.svc);
		}
		CHECK(At<u64>(bt.obj[kS18] + 0x20 + 0x18) == 4, "n=%d bits", n);
	}
}
// 3 -> 5 and 3 -> 6 (a never-reloaded Practice match keeps its dummy and gets P3/P4 on slots 3 and 4); 7 refused
void TestTargets()
{
	for (int target : { 5, 6 }) {
		Battle bt = BuildBattle(3);
		PlayFrames(bt, 3);
		const std::vector<Snap> before = Snapshot(bt, 3);
		u64 registry = bt.core;
		std::string err;
		GrowReport rep;
		const bool ok = GrowBattleTo(Addresses(&registry), target, err, &rep);
		CHECK(ok, "target %d: %s", target, err.c_str());
		if (!ok) continue;
		printf("%s\n", Describe(rep).c_str());
		CHECK(At<int32_t>(bt.core + 8) == target && rep.newCount == target, "target %d: count", target);
		for (const Snap& sn : before) {
			const u64 b = VecB(bt, sn.svc, sn.off);
			CHECK(VecE(bt, sn.svc, sn.off) - b == target * sn.elem, "target %d svc %d size", target, sn.svc);
			CHECK(!memcmp((void*)b, sn.bytes.data(), sn.bytes.size()), "target %d svc %d old changed", target, sn.svc);
			for (int k = 3; k < target; ++k) {
				const u64 el = b + k * sn.elem;
				if (sn.svc == kRound) { CHECK(At<u64>(At<u64>(el)) == kFreshEntryMark, "target %d round entry %d", target, k); continue; }
				bool fresh = true;
				for (u64 j = (sn.svc == kS1B && sn.off == 0x10) ? 8 : 0; j < sn.elem; ++j)
					if (At<uint8_t>(el + j) != FreshByte(sn.svc, sn.off, j)) { fresh = false; break; }
				CHECK(fresh, "target %d svc %d +0x%llX: element %d not fresh", target, sn.svc, sn.off, k);
			}
		}
		const u64 nb = VecB(bt, kS1B, 0x10);
		for (int k = 0; k < target; ++k) CHECK(At<u64>(At<u64>(nb + k * 0x28)) == nb + k * 0x28, "target %d node %d", target, k);
		for (u64 o : { 0x20ull, 0x40ull }) {
			const u64 hdr = bt.obj[kS18] + o;
			CHECK(At<u64>(hdr + 0x18) == (u64)target && (At<uint32_t>(At<u64>(hdr)) & ((1u << target) - 1)) == 0b101,
				"target %d bits +0x%llX", target, o);
		}
		CHECK(VecC(bt, kS14, 0x20) - VecB(bt, kS14, 0x20) == 0x400 * 0x40, "target %d: s14 reserve", target);
		// growing again to a smaller or equal count: nothing to do
		GrowReport rep2;
		CHECK(GrowBattleTo(Addresses(&registry), 4, err, &rep2) && rep2.newCount == target, "target %d: shrink call", target);
	}
	Battle bt = BuildBattle(3);
	u64 registry = bt.core;
	std::string err;
	CHECK(!GrowBattleTo(Addresses(&registry), 7, err, nullptr) && At<int32_t>(bt.core + 8) == 3, "target 7 accepted");
}
} // namespace

int main()
{
	TestGrow();
	TestRefusals();
	TestSmallCounts();
	TestTargets();
	if (g_failures) { printf("TagT8GrowTests: %d failure(s)\n", g_failures); return 1; }
	printf("TagT8GrowTests: all passed\n");
	return 0;
}
