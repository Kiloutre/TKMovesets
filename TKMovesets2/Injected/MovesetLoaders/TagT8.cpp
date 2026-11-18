// Tag mode for retail Tekken 8 Practice (see TagT8.hpp). Port of the RE project's scripts/runtime/tag.py (RUN_JS frame
// hook + the Python daemon) and fourp.py (P4 loading). Evidence for every address/offset: RE repo docs/tag-retail.md.
//
// Threading: every game call and every write to battle state happens on the game thread, inside the hooks (mostly at
// the end of the battle frame function). The loader's main loop thread only installs hooks and passes commands.

#include "TagT8.hpp"

#include <windows.h>
#include <tlhelp32.h>
#include <intrin.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <map>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "MovesetLoader.hpp"
#include "SharedMemory_t8.h"
#include "TagT8Grow.hpp"

namespace
{
// ---------------------------------------------------------------------------------------------------------------
// Logging (%TEMP%\TKMovesets2\tag.log) and status
// ---------------------------------------------------------------------------------------------------------------
std::mutex g_logMutex;
FILE* g_logFile = nullptr;
std::mutex g_statusMutex;
std::string g_status = "Tag: off";
std::atomic_int g_state = TagState_Off;

void Log(const char* fmt, ...)
{
	char buf[1024];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);
	DEBUG_LOG("[tag] %s\n", buf);
	std::lock_guard lock(g_logMutex);
	if (!g_logFile) {
		std::error_code ec;
		auto dir = std::filesystem::temp_directory_path(ec) / "TKMovesets2";
		std::filesystem::create_directories(dir, ec);
		g_logFile = _wfopen((dir / "tag.log").c_str(), L"a");
		if (!g_logFile) return;
	}
	SYSTEMTIME t; GetLocalTime(&t);
	fprintf(g_logFile, "%02d:%02d:%02d.%03d %s\n", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds, buf);
	fflush(g_logFile);
}

bool SetupInProgress();   // after the setup state machine
void SetStatus(int state, const char* fmt, ...)
{
	char buf[256];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);
	// the tool greys out Load/Disable while P3/P4 are being added (grow pending or a slot setup running)
	if (state == TagState_Running && SetupInProgress()) state = TagState_Loading;
	g_state = state;
	{
		std::lock_guard lock(g_statusMutex);
		if (g_status == buf) return;
		g_status = buf;
	}
	Log("status: %s", buf);
}

// ---------------------------------------------------------------------------------------------------------------
// Addresses (game_addresses.ini, t8-retail_tag_*)
// ---------------------------------------------------------------------------------------------------------------
struct Funcs {
	uint64_t frame, sceneCtor, setGamemode, sideFighter, activateSlot, activateSlotUE, getPlayer, getPlayer2,
		bindFighter, clearMoves, allocHelpers, setCpuStatus, setActive, initPlayer, initCpu, getMove, inputSource,
		sideButtons, wallSensor, psGet, psInit, motheadGetCopy, movesetRelocate, initMovesetParent, loaderWorld,
		getLoader, motionRequest, motionReady, rootDeltaPass, practiceReset, processEvent,
		malloc, battleCoreCtor, setBattleRegistry, setupBattle, loadMatchSetupRet, loadMatch, playerRoundInit, sideCostumeType, sideDescriptor, battleCoreDtor,
		animMapContains, binderCommonReady, animPopMap, motheadStoreRelease;
} F{};
struct Globals { uint64_t registry, roundKey, psKey, gamemode, motheadBinder, guobjectarray, namepool,
	animRefBind, umapRehash, vtAnimRef, vtAnimFactory; } G{};
struct Offsets {
	// player
	int slot, human, slot2, fighter, pos, facing, root, ready, mats, matT0, matT3, yaw, facing2, lookat, moveTimer,
		movePtr, vuln, animSpeed, moveId, inReaction, nextMovePtr, nextMoveId, inputMode, rootDelta, prevRoot, side,
		hpSlot, moveset, target, targeted, controller, clearedA, clearedC;
	// moveset / move
	int mTable, mCount, mSize, mCancel, mTransition, mLength, mActiveFirst, mActiveLast;
	// actor, managers, settings, round, registries
	int aFighter, aPlayerNumber, aRenderSlot, aMasterMesh, cmMap, dmMap, sDummyFighter, sDesc, sDescStride,
		sDescFighter, eToPlayer, rSlots, rIter, rStorage, rSlots2, rIter2, regSentinel, regBuckets, regMask,
		gmSentinel, gmCount, gmBuckets, gmMask, gmKey;
} O{};

MovesetLoader* g_loader = nullptr;
uint64_t g_base = 0, g_size = 0;
bool g_initialized = false;
bool g_hooked = false;

// ---------------------------------------------------------------------------------------------------------------
// Memory helpers
// ---------------------------------------------------------------------------------------------------------------
template<class T> inline T Rd(uint64_t a) { return *(volatile T*)a; }
template<class T> inline void Wr(uint64_t a, T v) { *(volatile T*)a = v; }
inline uint64_t P(uint64_t a) { return a ? Rd<uint64_t>(a) : 0; }
inline float Fx(uint64_t p) { return Rd<float>(p + O.pos); }
inline float Fz(uint64_t p) { return Rd<float>(p + O.pos + 8); }

void WritePos(uint64_t p, float x, float z)
{
	for (int o : { O.pos, O.root, O.matT0, O.matT3, O.prevRoot }) { Wr<float>(p + o, x); Wr<float>(p + o + 8, z); }
}
void MatchFloor(uint64_t dst, uint64_t src);
void CleanStalePlayer(uint64_t p, int slot);
int AnimDomainMapCount(uint32_t domain);

// ---------------------------------------------------------------------------------------------------------------
// Game functions
// ---------------------------------------------------------------------------------------------------------------
using fn_p_pi = uint64_t(*)(uint64_t, int*);
using fn_p = uint64_t(*)(uint64_t);
using fn_p_u8 = uint64_t(*)(uint64_t, uint8_t);
using fn_init_player = uint64_t(*)(uint64_t, uint64_t, int, uint8_t);
using fn_init_cpu = uint64_t(*)(uint64_t, uint8_t, uint8_t);
using fn_get_move = uint64_t(*)(uint64_t, uint32_t);
using fn_bind_fighter = uint64_t(*)(uint64_t, int*, int);
using fn_getcopy = uint64_t(*)(uint64_t, int, uint32_t);
using fn_relocate = uint64_t(*)(int, uint64_t, uint64_t);
using fn_init_mvp = uint64_t(*)(uint64_t, int, uint64_t, uint64_t);
using fn_p_u32_u32 = uint64_t(*)(uint64_t, uint32_t, uint32_t);
using fn_p_u32 = uint64_t(*)(uint64_t, uint32_t);
using fn_process_event = void(*)(uint64_t, uint64_t, void*);

template<class T> T Fn(uint64_t a) { return (T)a; }

uint64_t GetPlayer(uint64_t rnd, int slot) { int s = slot; return Fn<fn_p_pi>(F.getPlayer)(rnd, &s); }
uint64_t GetPlayer2(uint64_t rnd, int slot) { int s = slot; return Fn<fn_p_pi>(F.getPlayer2)(rnd, &s); }
uint64_t GetMove(uint64_t p, uint32_t id) { return Fn<fn_get_move>(F.getMove)(p, id); }
void InitCpu(uint64_t p, uint8_t cpu) { Fn<fn_init_cpu>(F.initCpu)(p, cpu, 0); }

// TK__GetStruct over the battle registry: the round (or the per-player service) for its key global
uint64_t GetStruct(uint64_t keyGlobal)
{
	const uint64_t reg = P(G.registry);
	if (!reg) return 0;
	const uint64_t r = P(reg + 0x10);
	if (!r) return 0;
	const uint32_t key = Rd<uint32_t>(keyGlobal);
	const uint64_t sentinel = P(r + O.regSentinel);
	const uint64_t b = P(r + O.regBuckets) + (Rd<uint64_t>(r + O.regMask) & key) * 16;
	uint64_t n = P(b + 8);
	for (int i = 0; i < 256 && n && n != sentinel; ++i) {
		if (Rd<uint32_t>(n + 0x10) == key) return P(n + 0x18);
		if (n == P(b)) break;
		n = P(n + 8);
	}
	return 0;
}

// Match settings (TK__GetGamemodeStruct): Practice dummy descriptor, per-side descriptors
uint64_t Settings()
{
	const uint64_t reg = P(G.gamemode);
	if (!reg || !Rd<uint64_t>(reg + O.gmCount)) return 0;
	const uint32_t key = Rd<uint32_t>(reg + O.gmKey);
	const uint64_t sentinel = P(reg + O.gmSentinel);
	const uint64_t bucket = P(reg + O.gmBuckets) + (key & Rd<uint64_t>(reg + O.gmMask)) * 16;
	const uint64_t first = P(bucket);
	uint64_t n = P(bucket + 8);
	if (n == sentinel) return 0;
	for (int i = 0; i < 256 && n; ++i) {
		if (Rd<uint32_t>(n + 0x10) == key) { const uint64_t v = P(n + 0x18); return v ? P(v + 8) : 0; }
		if (n == first) return 0;
		n = P(n + 8);
	}
	return 0;
}

// ---------------------------------------------------------------------------------------------------------------
// Unreal objects (FUObjectArray, FNamePool, reflection, ProcessEvent)
// ---------------------------------------------------------------------------------------------------------------
std::unordered_map<uint32_t, std::string> g_names;

const std::string& FName(uint32_t id)
{
	auto it = g_names.find(id);
	if (it != g_names.end()) return it->second;
	std::string s;
	const uint64_t block = P(G.namepool + 0x10 + (uint64_t)(id >> 16) * 8);
	if (block) {
		const uint64_t e = block + (uint64_t)(id & 0xFFFF) * 2;
		const uint16_t hdr = Rd<uint16_t>(e);
		const uint32_t len = hdr >> 6;
		if (len < 1024) {
			if (hdr & 1) { for (uint32_t i = 0; i < len; ++i) s.push_back((char)Rd<uint16_t>(e + 2 + i * 2)); }
			else s.assign((const char*)(e + 2), len);
		}
	}
	return g_names.emplace(id, std::move(s)).first->second;
}
const std::string& ObjName(uint64_t o) { return FName(Rd<uint32_t>(o + 0x18)); }
const std::string& ClassName(uint64_t o) { return ObjName(P(o + 0x10)); }

template<class Cb> void ForEachObject(Cb cb)
{
	const uint64_t oo = G.guobjectarray + 0x10;
	const uint64_t objects = P(oo);
	const int num = Rd<int32_t>(oo + 0x14);
	if (!objects) return;
	for (int i = 0; i < num; ++i) {
		const uint64_t chunk = P(objects + (uint64_t)(i >> 16) * 8);
		if (!chunk) { i |= 0xFFFF; continue; }
		const uint64_t o = P(chunk + (uint64_t)(i & 0xFFFF) * 0x18);
		if (!o) continue;
		if (!cb(o)) break;
	}
}

bool IsAliveUnsafe(uint64_t o);
// A cached object pointer may be freed memory (the previous match's objects): never fault on it
bool IsAlive(uint64_t o)
{
	if (!o) return false;
	__try { return IsAliveUnsafe(o); }
	__except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool IsAliveUnsafe(uint64_t o)
{
	const int idx = Rd<int32_t>(o + 0xC);
	const uint64_t objects = P(G.guobjectarray + 0x10);
	if (idx < 0 || idx >= Rd<int32_t>(G.guobjectarray + 0x10 + 0x14)) return false;
	const uint64_t chunk = P(objects + (uint64_t)(idx >> 16) * 8);
	return chunk && P(chunk + (uint64_t)(idx & 0xFFFF) * 0x18) == o;
}

bool IsDefaultObject(uint64_t o) { return (Rd<uint32_t>(o + 8) & 0x30) != 0; }   // RF_ClassDefaultObject | RF_ArchetypeObject

bool IsA(uint64_t o, const char* className)
{
	for (uint64_t c = P(o + 0x10); c; c = P(c + 0x40)) if (ObjName(c) == className) return true;
	return false;
}

uint64_t FindClass(const char* name)
{
	uint64_t found = 0;
	ForEachObject([&](uint64_t o) {
		if (ObjName(o) != name) return true;
		const auto& cn = ClassName(o);
		if (cn.size() >= 5 && cn.compare(cn.size() - 5, 5, "Class") == 0) { found = o; return false; }
		return true;
	});
	return found;
}

uint64_t FindFunc(uint64_t cls, const char* name)
{
	for (uint64_t c = cls; c; c = P(c + 0x40))
		for (uint64_t f = P(c + 0x48); f; f = P(f + 0x28))
			if (ObjName(f) == name) return f;
	return 0;
}

int PropOffset(uint64_t structOrFunc, const char* name)
{
	for (uint64_t c = structOrFunc; c; c = P(c + 0x40))
		for (uint64_t f = P(c + 0x50); f; f = P(f + 0x20))
			if (FName(Rd<uint32_t>(f + 0x28)) == name) return Rd<int32_t>(f + 0x4C);
	return -1;
}

struct UFunc { uint64_t func = 0; int size = 0; };
UFunc g_setHidden, g_setAnimClass, g_getCameraLocation;
int g_hiddenParamOff = 0, g_animClassParamOff = 0, g_cameraRetOff = 0;
int g_bHiddenOff = -1, g_animClassOff = -1, g_skinnedAssetOff = -1;
uint64_t g_cameraManager = 0;

bool CallUFunc(uint64_t obj, const UFunc& f, void* params)
{
	if (!obj || !f.func) return false;
	Fn<fn_process_event>(F.processEvent)(obj, f.func, params);
	return true;
}

void SetHidden(uint64_t actor, bool hidden)
{
	if (!actor || !IsAlive(actor) || !g_setHidden.func) return;
	uint8_t buf[64] = {};
	buf[g_hiddenParamOff] = hidden ? 1 : 0;
	CallUFunc(actor, g_setHidden, buf);
}

bool IsHidden(uint64_t actor) { return g_bHiddenOff >= 0 && (Rd<uint8_t>(actor + g_bHiddenOff) & 0x80); }

bool ResolveReflection()
{
	const uint64_t actor = FindClass("Actor"), smc = FindClass("SkeletalMeshComponent"), pcm = FindClass("PlayerCameraManager");
	if (!actor || !smc) return false;
	g_setHidden.func = FindFunc(actor, "SetActorHiddenInGame");
	g_setAnimClass.func = FindFunc(smc, "SetAnimClass");
	if (!g_setHidden.func || !g_setAnimClass.func) return false;
	g_setHidden.size = Rd<uint16_t>(g_setHidden.func + 0xB6);
	g_setAnimClass.size = Rd<uint16_t>(g_setAnimClass.func + 0xB6);
	g_hiddenParamOff = (std::max)(0, PropOffset(g_setHidden.func, "bNewHidden"));
	g_animClassParamOff = (std::max)(0, PropOffset(g_setAnimClass.func, "NewClass"));
	g_bHiddenOff = PropOffset(actor, "bHidden");
	g_animClassOff = PropOffset(smc, "AnimClass");
	g_skinnedAssetOff = PropOffset(smc, "SkinnedAsset");
	if (g_skinnedAssetOff <= 0) g_skinnedAssetOff = PropOffset(smc, "SkeletalMesh");
	if (pcm) {
		g_getCameraLocation.func = FindFunc(pcm, "GetCameraLocation");
		if (g_getCameraLocation.func) {
			g_getCameraLocation.size = Rd<uint16_t>(g_getCameraLocation.func + 0xB6);
			g_cameraRetOff = PropOffset(g_getCameraLocation.func, "ReturnValue");
		}
	}
	Log("reflection: SetActorHiddenInGame %llx (param +%d), SetAnimClass %llx, bHidden +0x%X, AnimClass +0x%X, GetCameraLocation %llx",
		g_setHidden.func, g_hiddenParamOff, g_setAnimClass.func, g_bHiddenOff, g_animClassOff, g_getCameraLocation.func);
	return g_hiddenParamOff + 1 <= 64 && g_bHiddenOff > 0;
}

bool IsCharacterActorClass(const std::string& cn)
{   // BP_Character_XXX_C
	return cn.size() == 18 && cn.compare(0, 13, "BP_Character_") == 0 && cn.compare(16, 2, "_C") == 0;
}

std::vector<uint64_t> CharacterActors()
{
	std::vector<uint64_t> out;
	ForEachObject([&](uint64_t o) {
		if (IsCharacterActorClass(ClassName(o)) && !IsDefaultObject(o) && !(Rd<uint32_t>(o + 8) & 0x20000000)) out.push_back(o);
		return true;
	});
	return out;
}

uint64_t FindInstanceOf(const char* className)
{
	uint64_t found = 0;
	ForEachObject([&](uint64_t o) {
		if (!IsDefaultObject(o) && IsA(o, className)) { found = o; return false; }
		return true;
	});
	return found;
}

uint64_t FindCameraManager()
{
	uint64_t found = 0;
	ForEachObject([&](uint64_t o) {
		const auto& cn = ClassName(o);
		if (cn.size() >= 19 && cn.compare(cn.size() - 19, 19, "PlayerCameraManager") == 0 && !IsDefaultObject(o)
			&& ObjName(o).find("Default") == std::string::npos) { found = o; return false; }
		return true;
	});
	return found;
}

bool CameraBattle(float& cx, float& cz)
{
	if (!g_cameraManager || !IsAlive(g_cameraManager)) g_cameraManager = FindCameraManager();
	if (!g_cameraManager || !g_getCameraLocation.func || g_cameraRetOff < 0) return false;
	alignas(16) uint8_t buf[128] = {};
	if (g_getCameraLocation.size > (int)sizeof(buf)) return false;
	CallUFunc(g_cameraManager, g_getCameraLocation, buf);
	double wx, wy;
	memcpy(&wx, buf + g_cameraRetOff, 8); memcpy(&wy, buf + g_cameraRetOff + 8, 8);
	// UE world (X, Y) = (battle z, -battle x) / 10 (retail, measured 2026-11-12)
	cx = (float)(-10.0 * wy); cz = (float)(10.0 * wx);
	return true;
}

// ---------------------------------------------------------------------------------------------------------------
// Moves: retail name decryption (LukzB), run loops, lookups
// ---------------------------------------------------------------------------------------------------------------
inline uint64_t Rol(uint64_t k, unsigned n) { n &= 63; return n ? (k << n) | (k >> (64 - n)) : k; }

uint32_t DecryptName(uint64_t value, uint64_t key)
{
	uint64_t chk = 0, x = value & 0xFFFFFFFF;
	for (unsigned sh = 0; sh < 32; sh += 8) {
		chk ^= (x & 0xFFFFFFFF) ^ (Rol(key, sh + 8) & 0xFFFFFFFF);
		x >>= 8;
	}
	if (!chk) chk = 1;
	if (((value & 0xFFFFFFFF) | ((chk & 0xFFFFFFFF) << 32)) != value) return 0;
	const uint64_t s = value ^ 0x1D;
	const uint64_t k = Rol(key, (unsigned)(s & 0x1F)) & 0xFFFFFFFFFFFFFFE0ull;
	return (uint32_t)((k ^ s) & 0xFFFFFFFF);
}

uint32_t MoveName(uint64_t mv) { return DecryptName(Rd<uint64_t>(mv), Rd<uint64_t>(mv + 8)); }

constexpr uint32_t H_LOOP_A = 0xB4050732, H_LOOP_B = 0x2B658105, H_STOP_R = 0xDB3F4798, H_DUM_A = 0x6FC7F4C7,
	H_DUM_B = 0x5AB45EFD, H_DUM_C = 0x999CE596, H_DUM_D = 0x58EA7934;
constexpr uint32_t BACKDASH[] = { 0x5177DEE6, 0xE0ADE302, 0x815F95F8 };
constexpr int NAME_LEN = 16;          // retail move name: (encrypted value, key)
constexpr int CANCEL_CACHE_END = 0xCC; // move 0's cancel caches are copied from +cancel to here

void* GameHeapAlloc(size_t n) { return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, n); }   // outlives the DLL

struct MoveSet {
	int run = -1, runInt = -1, dash = -1;
	uint64_t runPtr = 0, runIntPtr = 0, idle = 0, dashPtr = 0;
};
std::map<uint64_t, MoveSet> g_moves;   // player -> its run-in moves

int FindMoveIndex(uint64_t ms, std::initializer_list<uint32_t> hashes)
{
	const uint64_t base = P(ms + O.mTable);
	const int n = Rd<int32_t>(ms + O.mCount);
	for (int i = 0; i < n; ++i) {
		const uint32_t h = MoveName(base + (uint64_t)i * O.mSize);
		for (auto w : hashes) if (h == w) return i;
	}
	return -1;
}

// Endless run in Co_Dummy_55/56: the run loop with end-only cancel lists pointing at each other (install_runloop)
int InstallRunLoop(uint64_t ms)
{
	const uint64_t base = P(ms + O.mTable);
	auto mv = [&](int i) { return base + (uint64_t)i * O.mSize; };
	const int la = FindMoveIndex(ms, { H_LOOP_A }), lb = FindMoveIndex(ms, { H_LOOP_B });
	const int a = FindMoveIndex(ms, { H_DUM_A }), b = FindMoveIndex(ms, { H_DUM_B });
	if (la < 0 || lb < 0 || a < 0 || b < 0) return -1;
	if (Rd<uint16_t>(mv(a) + O.mTransition) != b) {
		uint64_t c = P(mv(la) + O.mCancel);
		for (int k = 0; k < 4096 && Rd<uint32_t>(c) != 0x8000; ++k) c += 0x28;
		uint8_t end[0x28]; memcpy(end, (void*)c, 0x28);
		uint8_t* recs = (uint8_t*)GameHeapAlloc(0x50);
		for (int k = 0; k < 2; ++k) { memcpy(recs + 0x28 * k, end, 0x28); *(uint16_t*)(recs + 0x28 * k + 0x24) = (uint16_t)(k == 0 ? b : a); }
		const std::pair<int, int> pairs[2] = { { a, la }, { b, lb } };
		for (int k = 0; k < 2; ++k) {
			const int dst = pairs[k].first, src = pairs[k].second;
			std::vector<uint8_t> rec(O.mSize);
			memcpy(rec.data(), (void*)mv(src), O.mSize);
			memcpy(rec.data(), (void*)mv(dst), NAME_LEN);                                       // keep the dummy's name
			memcpy(rec.data() + O.mCancel, (void*)(base + O.mCancel), CANCEL_CACHE_END - O.mCancel); // move 0's caches
			*(uint64_t*)(rec.data() + O.mCancel) = (uint64_t)(recs + 0x28 * k);
			*(uint16_t*)(rec.data() + O.mTransition) = (uint16_t)(dst == a ? b : a);
			memcpy((void*)mv(dst), rec.data(), O.mSize);
		}
	}
	return a;
}

// Interruptible run (tag combo) in Co_Dummy_53/54: the run's cancel list minus its input-less entries
int InstallRunLoopInterruptible(uint64_t ms)
{
	const uint64_t base = P(ms + O.mTable);
	auto mv = [&](int i) { return base + (uint64_t)i * O.mSize; };
	const int la = FindMoveIndex(ms, { H_LOOP_A }), lb = FindMoveIndex(ms, { H_LOOP_B });
	const int c_ = FindMoveIndex(ms, { H_DUM_C }), d_ = FindMoveIndex(ms, { H_DUM_D });
	if (la < 0 || lb < 0 || c_ < 0 || d_ < 0) return -1;
	if (Rd<uint16_t>(mv(c_) + O.mTransition) != d_) {
		const std::pair<int, int> pairs[2] = { { c_, la }, { d_, lb } };
		for (int k = 0; k < 2; ++k) {
			const int dst = pairs[k].first, src = pairs[k].second, other = k == 0 ? d_ : c_;
			std::vector<uint8_t> recs;
			uint64_t lst = P(mv(src) + O.mCancel);
			for (int n = 0; n < 4096; ++n, lst += 0x28) {
				uint8_t r[0x28]; memcpy(r, (void*)lst, 0x28);
				const uint32_t lo = *(uint32_t*)r;
				if (lo == 0x8000) { *(uint16_t*)(r + 0x24) = (uint16_t)other; recs.insert(recs.end(), r, r + 0x28); break; }
				if (lo != 0) recs.insert(recs.end(), r, r + 0x28);
			}
			uint8_t* mem = (uint8_t*)GameHeapAlloc(recs.size());
			memcpy(mem, recs.data(), recs.size());
			std::vector<uint8_t> rec(O.mSize);
			memcpy(rec.data(), (void*)mv(src), O.mSize);
			memcpy(rec.data(), (void*)mv(dst), NAME_LEN);
			*(uint64_t*)(rec.data() + O.mCancel) = (uint64_t)mem;
			*(uint16_t*)(rec.data() + O.mTransition) = (uint16_t)other;
			memcpy((void*)mv(dst), rec.data(), O.mSize);
		}
	}
	return c_;
}

void QueueMove(uint64_t p, uint32_t id, uint64_t mp)
{
	Wr<uint32_t>(p + O.nextMoveId, id);
	Wr<uint64_t>(p + O.nextMovePtr, mp);
	const uint64_t cur = P(p + O.movePtr);
	if (cur) Wr<uint32_t>(p + O.moveTimer, Rd<uint32_t>(cur + O.mLength));
}

// ---------------------------------------------------------------------------------------------------------------
// Tag runtime state (game thread only)
// ---------------------------------------------------------------------------------------------------------------
constexpr int TEAM_POINT[2] = { 0, 1 };
int g_partnerSlot[2] = { 2, 3 };       // [team] -> partner slot, -1 = no partner
constexpr float STANDBY_OFF = 6000.f, LEAVE_EXTRA = 2500.f, EASE = 0.2f, STANDBY_X = 900.f, STANDBY_LANE = 1300.f;
constexpr float ENTER_BEHIND = 2500.f, ENTER_LANE = 700.f, ARRIVE_MIN = 900.f, RUN_SPEED = 90.f;
constexpr float COMBO_ARRIVE = 1100.f, COMBO_LANE = 300.f, COMBO_BEHIND = 1200.f, LEAVE_PUSH = 30.f;
constexpr int HOLD = 10, SNAP = 10, PLACE_FRAMES = 3, QUEUE_MAX = 15, LEAVE_SLOW = 0x600;

std::atomic_bool g_enabled = false;
std::atomic_int g_p3 = -1, g_p4 = -1;
std::atomic_bool g_kiRequest[2] = { false, false };
bool g_kiLatch[2] = { false, false };
bool g_practiceDummy = false;          // the last SetGamemode built a Practice dummy (slot 2)
bool g_p4Unavailable = false;          // P4 asked for, but the running battle has 3 entries
std::atomic_int g_inHook = 0;          // hook bodies running right now (unload waits for 0)
std::atomic_bool g_shuttingDown = false;
bool g_inPracticeReset = false;        // inside the View+A restart routine (game thread)
TagT8Grow::GrowAddresses g_grow;       // in-place 3 -> 4 grow (TagT8Grow.cpp, grow-in-place session)
bool g_growReady = false;
bool g_growPending = false;
// Reload mode (battle built with 4 entries at Practice entry / Quick Select: P3 = the dummy, P4 loaded at load) is
// switched off (owner, 2026-11-15): the extra entries it reuses across matches keep stale state (NaN placement,
// dangling skeleton callbacks) and reloads crashed. Tag runs only in the in-place grow mode (P3 slot 3, P4 slot 4)
// and switches off at a reload. The reload-mode code paths stay, gated by this flag.
constexpr bool kReloadMode = false;
int g_growTarget = 4;          // slots the in-place grow makes
int g_growMax = 5;             // P3 on slot 3 + P4 on slot 4 (TagT8Grow supports up to kMaxTarget)
struct HookGuard { HookGuard() { ++g_inHook; } ~HookGuard() { --g_inHook; } };

uint64_t g_rnd = 0;                    // round struct (GetStruct(round key))
uint64_t g_matchRound = 0;             // *g_rnd of the match being set up / running
bool g_running = false;
int g_frames = 0;
int g_snap = 0;
int g_restartFrame = -1000;
bool g_muteRestart = false;
int g_reinitAt = -1;                   // frame at which the partners are re-initialised after a Practice restart

uint64_t g_dm = 0, g_cm = 0, g_settings = 0;
std::map<uint64_t, uint64_t> g_actor;  // player -> its actor
std::map<uint64_t, bool> g_beingHit;
uint64_t g_neutral[4] = {};             // players whose input is forced neutral

inline uint64_t RoundObj() { return g_rnd ? P(g_rnd) : 0; }
uint64_t Entry(int slot)
{
	const uint64_t r = RoundObj();
	if (!r || slot < 0) return 0;
	const uint64_t e = P(P(r + O.rSlots) + 8ull * slot);
	return e ? e + O.eToPlayer : 0;
}
uint64_t Entry2(int slot)
{
	const uint64_t r = RoundObj();
	if (!r || slot < 0) return 0;
	const uint64_t e = P(P(r + O.rSlots2) + 8ull * slot);
	return e ? e + O.eToPlayer : 0;
}
int EntryCount()
{
	const uint64_t r = RoundObj();
	return r ? (int)((P(r + O.rStorage + 8) - P(r + O.rStorage)) / 8) : 0;
}
uint64_t EntryAddr(int idx)
{
	const uint64_t r = RoundObj();
	return r ? P(P(r + O.rStorage) + 8ull * idx) : 0;
}

bool IsPartner(uint64_t p)
{
	if (!g_running || !p) return false;
	for (int t = 0; t < 2; ++t) if (g_partnerSlot[t] >= 0 && Entry(g_partnerSlot[t]) == p) return true;
	return false;
}

void SetNeutral(uint64_t p, bool on)
{
	for (auto& n : g_neutral) if (n == p) { if (!on) n = 0; return; }
	if (on) for (auto& n : g_neutral) if (!n) { n = p; return; }
}
bool IsNeutral(uint64_t p) { for (auto n : g_neutral) if (n && n == p) return true; return false; }

bool Reacting(uint64_t p) { return Rd<uint32_t>(p + O.inReaction) != 0 || (Rd<uint32_t>(p + O.vuln) & 7) == 6; }
bool Airborne(uint64_t p) { return (Rd<uint32_t>(p + O.vuln) & 7) == 6; }
void UpdateHit(uint64_t p)
{
	if (!p) return;
	const uint32_t mv = Rd<uint32_t>(p + O.moveId);
	if (Reacting(p)) g_beingHit[p] = true;
	else if (mv >= 0x8001 && mv <= 0x8003) g_beingHit[p] = false;
}

void Axis(uint64_t p, uint64_t e, float& ux, float& uz)
{
	const float dx = Fx(p) - Fx(e), dz = Fz(p) - Fz(e);
	float n = std::hypot(dx, dz); if (n == 0) n = 1;
	ux = dx / n; uz = dz / n;
}
float Dist(uint64_t p, uint64_t q) { return std::hypot(Fx(p) - Fx(q), Fz(p) - Fz(q)); }

// (x, z) `dist` mm behind `point` on the enemy->point axis, `lane` mm sideways away from the camera
void Behind(uint64_t point, uint64_t enemy, float dist, float lane, float& ox, float& oz)
{
	const float px = Fx(point), pz = Fz(point);
	float ux, uz; Axis(point, enemy, ux, uz);
	float wx = -uz, wz = ux;
	if (lane != 0) {
		float cx, cz;
		if (CameraBattle(cx, cz) && wx * (px - cx) + wz * (pz - cz) < 0) { wx = -wx; wz = -wz; }
	}
	ox = px + ux * dist + wx * lane; oz = pz + uz * dist + wz * lane;
}

// Orientation of `p` turned toward `target` as if it stood at (x, z): matrices rows 0/2, yaw, both facing words
struct Orient { bool any = false; uint8_t mats[0x100]; float yaw; uint16_t facing, facing2; };
Orient OrientedTowardFrom(uint64_t p, float x, float z, uint64_t target)
{
	Orient o;
	memcpy(o.mats, (void*)(p + O.mats), 0x100);
	const float fx = *(float*)(o.mats + 0x20), fz = *(float*)(o.mats + 0x28);
	const float dx = Fx(target) - x, dz = Fz(target) - z;
	constexpr double PI = 3.14159265358979323846;
	double th = std::atan2(dz, dx) - std::atan2(fz, fx);
	th = std::fmod(th + PI, 2 * PI); if (th < 0) th += 2 * PI; th -= PI;
	if (!std::isfinite(th) || !std::isfinite(Rd<float>(p + O.yaw)) || std::fabs(th) < 2 * PI / 180) return o;
	const double c = std::cos(th), s = std::sin(th);
	for (int m = 0; m < 4; ++m) for (int row : { 0, 2 }) {
		float* v = (float*)(o.mats + m * 0x40 + row * 0x10);
		const double a = v[0], b = v[2];
		v[0] = (float)(a * c - b * s); v[2] = (float)(a * s + b * c);
	}
	constexpr double ROT_SIGN = -1.0;   // facing words turn opposite to the matrix rotation (live)
	double yaw = Rd<float>(p + O.yaw) + ROT_SIGN * th;
	yaw = std::fmod(yaw + PI, 2 * PI); if (yaw < 0) yaw += 2 * PI; yaw -= PI;
	o.yaw = (float)yaw;
	const int dw = (int)std::lround(ROT_SIGN * th * 65536 / (2 * PI));
	o.facing = (uint16_t)(Rd<uint16_t>(p + O.facing) + dw);
	o.facing2 = (uint16_t)(Rd<uint16_t>(p + O.facing2) + dw);
	o.any = true;
	return o;
}
void WriteOrient(uint64_t p, const Orient& o)
{
	if (!o.any) return;
	memcpy((void*)(p + O.mats), o.mats, 0x100);
	Wr<float>(p + O.yaw, o.yaw);
	Wr<uint16_t>(p + O.facing, o.facing);
	Wr<uint16_t>(p + O.facing2, o.facing2);
}

struct SwapInfo {
	bool valid = false;
	int a = 0, b = 0;
	uint64_t x = 0, y = 0, e = 0, x2 = 0, y2 = 0, ax = 0, ay = 0, dm = 0, cm = 0, s = 0;
	int fx = 0, fy = 0;
	bool human = false;
};

uint64_t ActorOf(uint64_t p) { auto it = g_actor.find(p); return it == g_actor.end() ? 0 : it->second; }
int Fighter(uint64_t p) { return Rd<int32_t>(p + O.fighter); }

// The swap of tag(): bindings of slots a <-> b (slot vectors, ids, actors, managers, targets, input, CPU, descriptors)
void DoSwap(const SwapInfo& w)
{
	const uint64_t r = RoundObj();
	// slot -> entry tables (64 wide): by slot index
	for (int v : { O.rSlots, O.rSlots2 }) {
		const uint64_t base = P(r + v);
		const uint64_t qa = P(base + 8ull * w.a), qb = P(base + 8ull * w.b);
		Wr<uint64_t>(base + 8ull * w.a, qb); Wr<uint64_t>(base + 8ull * w.b, qa);
	}
	// iteration vectors list only the ACTIVE entries, in activation order: swap the two where they sit. By index it
	// wrote past the end once the Practice dummy stayed inactive (slot 4 at position 3: heap corruption, frozen game,
	// 2026-11-14 23:02)
	for (int v : { O.rIter, O.rIter2 }) {
		const uint64_t begin = P(r + v), end = P(r + v + 8);
		const uint64_t ea = P(P(r + (v == O.rIter ? O.rSlots : O.rSlots2)) + 8ull * w.b);   // after the table swap above
		const uint64_t eb = P(P(r + (v == O.rIter ? O.rSlots : O.rSlots2)) + 8ull * w.a);
		uint64_t ia = 0, ib = 0;
		for (uint64_t it = begin; it < end && it < begin + 64 * 8; it += 8) {
			const uint64_t e = P(it);
			if (e == ea) ia = it; else if (e == eb) ib = it;
		}
		if (ia && ib) { Wr<uint64_t>(ia, eb); Wr<uint64_t>(ib, ea); }
	}
	const std::pair<uint64_t, uint64_t> pairs[2] = { { w.x, w.y }, { w.x2, w.y2 } };
	for (auto [u, v] : pairs) {
		if (!u || !v) continue;
		for (int o : { O.slot, O.slot2, O.hpSlot }) { const uint32_t a_ = Rd<uint32_t>(u + o), b_ = Rd<uint32_t>(v + o); Wr(u + o, b_); Wr(v + o, a_); }
	}
	if (w.ax) { Wr<int32_t>(w.ax + O.aRenderSlot, w.b); Wr<int32_t>(w.ax + O.aPlayerNumber, w.b); }
	if (w.ay) { Wr<int32_t>(w.ay + O.aRenderSlot, w.a); Wr<int32_t>(w.ay + O.aPlayerNumber, w.a); }
	if (w.dm) {          // TMap<int, TWeakObjectPtr> elements {key, index, serial, next, hash} 0x14
		const uint64_t data = P(w.dm + O.dmMap);
		const int n = Rd<int32_t>(w.dm + O.dmMap + 8);
		uint64_t ea = 0, eb = 0;
		for (int i = 0; i < n && i < 64; ++i) {
			const int k = Rd<int32_t>(data + i * 0x14ull);
			if (k == w.a) ea = data + i * 0x14ull + 4;
			if (k == w.b) eb = data + i * 0x14ull + 4;
		}
		if (ea && eb) { const uint64_t t1 = Rd<uint64_t>(ea), t2 = Rd<uint64_t>(eb); Wr(ea, t2); Wr(eb, t1); }
	}
	if (w.cm) {          // TMap<int, actor*> elements 0x18
		const uint64_t data = P(w.cm + O.cmMap);
		const int n = Rd<int32_t>(w.cm + O.cmMap + 8);
		for (int i = 0; i < n && i < 64; ++i) {
			const int k = Rd<int32_t>(data + i * 0x18ull);
			if (k == w.a && w.ay) Wr<uint64_t>(data + i * 0x18ull + 8, w.ay);
			else if (k == w.b && w.ax) Wr<uint64_t>(data + i * 0x18ull + 8, w.ax);
		}
	}
	for (int o : { O.target, O.targeted }) { Wr<uint64_t>(w.e + o, w.y); Wr<uint64_t>(w.y + o, w.e); Wr<uint64_t>(w.x + o, w.e); }
	Wr<int32_t>(w.y + O.inputMode, 2);
	InitCpu(w.y, w.human ? 0 : 1);
	InitCpu(w.x, 1);
	Wr<uint32_t>(w.s + O.sDesc + (uint64_t)O.sDescStride * w.a + O.sDescFighter, (uint32_t)w.fy);
	Wr<uint32_t>(w.s + O.sDesc + (uint64_t)O.sDescStride * w.b + O.sDescFighter, (uint32_t)w.fx);
}

SwapInfo MakeSwap(int team)
{
	// point slot a (going out) <-> partner slot b (coming in), from the current layout
	SwapInfo w;
	const int a = TEAM_POINT[team], b = g_partnerSlot[team];
	const uint64_t x = Entry(a), y = Entry(b);
	if (!x || !y) return w;
	w.valid = true; w.a = a; w.b = b; w.x = x; w.y = y; w.e = Entry(1 - a);
	w.x2 = Entry2(a); w.y2 = Entry2(b);
	w.ax = ActorOf(x); w.ay = ActorOf(y);
	w.dm = g_dm; w.cm = g_cm; w.s = g_settings;
	w.fx = Fighter(x); w.fy = Fighter(y);
	w.human = Rd<int32_t>(x + O.controller) == 0;
	return w;
}

enum class Phase { Enter, Leave, Done };
struct Transition {
	bool active = false;
	Phase phase = Phase::Enter;
	uint64_t x = 0, y = 0, e = 0;
	float arrive = 0; int t = 0;
	uint32_t dashId = 0; uint64_t dashPtr = 0;
	float ux = 0, uz = 0, dx = 0, dz = 0;
	bool combo = false;
	int placeN = 0; float placeX = 0, placeZ = 0; Orient orient;
	uint32_t runId = 0; uint64_t runPtr = 0;
	uint32_t stopId = 0; uint64_t stopPtr = 0;
	SwapInfo sw;
	bool drive = false; float driveX = 0, driveZ = 0;
	int holdN = 0; float holdX = 0, holdZ = 0;
} g_tr[2];

struct Queued { bool active = false; int press = 0; uint32_t id = 0, f = 0; bool er = false; uint32_t eid = 0, ef = 0; } g_tq[2];
SwapInfo g_revert[2];
bool g_swapped[2] = { false, false };
bool g_comboTeam[2] = { false, false };
int g_heldSince[2] = { -1, -1 };
bool g_pendingTag[2] = { false, false };

bool IsRunId(const Transition& s, uint32_t id) { return id == s.runId || id == s.runId + 1; }

void Finish(int team)
{
	auto& s = g_tr[team];
	if (s.active) { SetNeutral(s.x, false); SetNeutral(s.y, false); }
	s = Transition{};
}

// ---- requests: Ki Charge / X+Y+A+B -> queued while the point attacks (fires on contact or at the move's end)
struct Attack { bool any = false; uint32_t a0 = 0, a1 = 0, f = 0, id = 0; };
Attack AttackOf(uint64_t p)
{
	Attack at;
	const uint64_t mp = P(p + O.movePtr);
	if (!mp) return at;
	at.a0 = Rd<uint32_t>(mp + O.mActiveFirst); at.a1 = Rd<uint32_t>(mp + O.mActiveLast);
	if (at.a1 == 0 || at.a1 > 1000) return at;
	at.any = true; at.f = Rd<uint32_t>(p + O.moveTimer); at.id = Rd<uint32_t>(p + O.moveId);
	return at;
}

void RequestTag(int team)
{
	if (g_partnerSlot[team] < 0) return;
	const uint64_t p = Entry(TEAM_POINT[team]), e = Entry(1 - TEAM_POINT[team]);
	if (!p || !e) return;
	const Attack at = g_tr[team].active ? Attack{} : AttackOf(p);
	if (!at.any) { g_pendingTag[team] = true; return; }
	if (at.f > at.a1 && Reacting(e)) { g_pendingTag[team] = true; return; }
	auto& q = g_tq[team];
	if (q.active) { q.press = g_frames; return; }
	q = Queued{ true, g_frames, at.id, at.f, Reacting(e), Rd<uint32_t>(e + O.moveId), Rd<uint32_t>(e + O.moveTimer) };
	Log("tag %c queued: the point is attacking (move %u frame %u, active %u-%u)", 'A' + team, at.id, at.f, at.a0, at.a1);
}

void CheckQueue()
{
	for (int t = 0; t < 2; ++t) {
		auto& q = g_tq[t];
		if (!q.active) continue;
		const uint64_t p = Entry(TEAM_POINT[t]), e = Entry(1 - TEAM_POINT[t]);
		if (g_frames - q.press > QUEUE_MAX) { q.active = false; Log("queued tag %c dropped", 'A' + t); continue; }
		const bool er = Reacting(e);
		const uint32_t eid = Rd<uint32_t>(e + O.moveId), ef = Rd<uint32_t>(e + O.moveTimer);
		const bool contact = er && (!q.er || eid != q.eid || ef < q.ef);
		const uint32_t id = Rd<uint32_t>(p + O.moveId), f = Rd<uint32_t>(p + O.moveTimer);
		if (contact || id != q.id || f < q.f) {
			q.active = false; g_pendingTag[t] = true;
			Log("queued tag %c fires: %s %d frames after the press", 'A' + t, contact ? "contact" : "move end", g_frames - q.press);
		}
		else { q.er = er; q.eid = eid; q.ef = ef; q.f = f; }
	}
}

bool SaneState(uint64_t p)
{
	auto ok = [](float v) { return std::isfinite(v) && std::fabs(v) < 1e8f; };
	if (!ok(Rd<float>(p + O.yaw))) return false;
	for (int o : { O.pos, O.root, O.matT0, O.matT3, O.prevRoot }) for (int c : { 0, 4, 8 }) if (!ok(Rd<float>(p + o + c))) return false;
	return true;
}

// Replace every non-finite (or absurd) placement float of `p` with the point `from`'s: yaw, facing words, matrices,
// position/root, previous-root block and root delta. The incoming fighter is then placed by the tag as usual.
void RepairState(uint64_t p, uint64_t from)
{
	auto bad = [](float v) { return !std::isfinite(v) || std::fabs(v) >= 1e8f; };
	auto fix = [&](int o, int n) { for (int i = 0; i < n; i += 4) if (bad(Rd<float>(p + o + i))) Wr<uint32_t>(p + o + i, Rd<uint32_t>(from + o + i)); };
	if (bad(Rd<float>(p + O.yaw))) {
		Wr<float>(p + O.yaw, Rd<float>(from + O.yaw));
		Wr<uint16_t>(p + O.facing, Rd<uint16_t>(from + O.facing));
		Wr<uint16_t>(p + O.facing2, Rd<uint16_t>(from + O.facing2));
	}
	fix(O.mats, 0x100);
	fix(O.pos, 0xC); fix(O.root, 0xC);
	fix(O.prevRoot, 0x70);
	fix(O.rootDelta, 0x20);
}

// Start a run-in tag (or a tag combo when the enemy is airborne). Python's run() "tag" branch + beginRun.
void BeginTag(int team)
{
	const int a = TEAM_POINT[team], b = g_partnerSlot[team];
	const uint64_t x = Entry(a), y = Entry(b), e = Entry(1 - a);
	if (!x || !y || !e) return;
	if (g_beingHit[x] || Reacting(x)) { Log("tag %c refused: the point is being hit", 'A' + team); g_heldSince[team] = -1; return; }
	if (Airborne(e)) {
		// Ki Charge on a launcher's impact frame: swapping during the hit resolution froze the game. Wait 2 frames.
		if (g_heldSince[team] < 0) g_heldSince[team] = g_frames;
		if (g_frames < g_heldSince[team] + 2) { g_pendingTag[team] = true; return; }
	}
	g_heldSince[team] = -1;
	if (g_tr[team].active) return;
	// A partner whose simulation went NaN (live 2026-11-14 23:28: a P4 loaded at match load had NaN yaw, root delta and
	// matrix Y) freezes the game in the camera's angle-wrap loop 0x141811752 once it is the point: never bring it in.
	if (!SaneState(y)) { RepairState(y, x); Log("tag %c: partner placement was NaN, repaired from the point", 'A' + team); }
	auto itx = g_moves.find(x), ity = g_moves.find(y);
	if (itx == g_moves.end() || ity == g_moves.end() || ity->second.run < 0) { Log("tag %c: run moves missing", 'A' + team); return; }
	const bool combo = Airborne(e);
	float px, pz;
	Behind(x, e, combo ? COMBO_BEHIND : ENTER_BEHIND, combo ? COMBO_LANE : ENTER_LANE, px, pz);
	const float d0 = Dist(x, e);
	auto& s = g_tr[team];
	s = Transition{};
	s.active = true; s.phase = Phase::Enter; s.x = x; s.y = y; s.e = e;
	s.combo = combo;
	s.arrive = combo ? COMBO_ARRIVE : (std::max)(d0, ARRIVE_MIN);
	Axis(x, e, s.ux, s.uz);
	{ const float dx = Fx(e) - px, dz = Fz(e) - pz; float n = std::hypot(dx, dz); if (n == 0) n = 1; s.dx = dx / n; s.dz = dz / n; }
	s.placeN = PLACE_FRAMES; s.placeX = px; s.placeZ = pz; s.orient = OrientedTowardFrom(y, px, pz, e);
	const MoveSet& my = ity->second;
	const MoveSet& mx = itx->second;
	s.runId = (uint32_t)(combo && my.runInt >= 0 ? my.runInt : my.run);
	s.runPtr = combo && my.runInt >= 0 ? my.runIntPtr : my.runPtr;
	s.stopId = 0x8001; s.stopPtr = my.idle;
	s.dashId = mx.dash >= 0 ? (uint32_t)mx.dash : 0; s.dashPtr = mx.dash >= 0 ? mx.dashPtr : 0;
	s.sw = MakeSwap(team);
	SetNeutral(x, true);
	if (!combo) SetNeutral(y, true);
	SetHidden(ActorOf(y), false);
	g_comboTeam[team] = combo;
	Log("tag %c %s (point %.0f mm from the enemy)", 'A' + team, combo ? "combo" : "run-in", d0);
}

void Follow(uint64_t point, uint64_t partner, uint64_t enemy)
{
	float ux, uz; Axis(point, enemy, ux, uz);
	const float tx = Fx(point) + ux * STANDBY_OFF, tz = Fz(point) + uz * STANDBY_OFF;
	const float k = g_snap > 0 ? 1.f : EASE;
	WritePos(partner, Fx(partner) + (tx - Fx(partner)) * k, Fz(partner) + (tz - Fz(partner)) * k);
}

void OnSwapped(int team)
{
	g_swapped[team] = !g_swapped[team];
	g_revert[team] = g_swapped[team] ? MakeSwap(team) : SwapInfo{};
	g_comboTeam[team] = false;
	Log("tag %c: fighter %d in", 'A' + team, Fighter(Entry(TEAM_POINT[team])));
}

void OnLeft(int team)
{
	const uint64_t out = Entry(g_partnerSlot[team]);        // after the swap the outgoing fighter is the partner
	if (out) { Wr<int32_t>(out + O.inputMode, 0); SetHidden(ActorOf(out), true); }
	Finish(team);
}

void UpdateTransitions()
{
	for (int t = 0; t < 2; ++t) {
		auto& s = g_tr[t];
		if (!s.active) continue;
		s.t++;
		if (!SaneState(s.y)) RepairState(s.y, s.x);
		if (s.placeN > 0) {                       // runner placement + orientation, on the point's floor
			WriteOrient(s.y, s.orient); MatchFloor(s.y, s.x); WritePos(s.y, s.placeX, s.placeZ);
			if (--s.placeN == 0) {
				if (s.combo && s.sw.valid) { DoSwap(s.sw); SetNeutral(s.y, false); OnSwapped(t); }
				QueueMove(s.y, s.runId, s.runPtr);
				if (s.dashPtr) QueueMove(s.x, s.dashId, s.dashPtr);
				s.drive = true; s.driveX = s.placeX; s.driveZ = s.placeZ;
			}
		}
		if (s.drive) {
			if (s.combo && !IsRunId(s, Rd<uint32_t>(s.y + O.moveId))) { s.drive = false; s.phase = Phase::Leave; s.t = 0; }
			else { s.driveX += s.dx * RUN_SPEED; s.driveZ += s.dz * RUN_SPEED; WritePos(s.y, s.driveX, s.driveZ); }
		}
		if (s.holdN > 0) { WritePos(s.y, s.holdX, s.holdZ); s.holdN--; }
		// leaving: one backdash, slowed (anim speed reset by the game at move start, so held), pushed away
		if (s.dashId && Rd<uint32_t>(s.x + O.moveId) == s.dashId) Wr<int32_t>(s.x + O.animSpeed, LEAVE_SLOW);
		{
			float ux, uz; Axis(s.x, s.e, ux, uz);
			for (int o : { O.pos, O.root, O.matT0, O.matT3, O.prevRoot }) {
				Wr<float>(s.x + o, Rd<float>(s.x + o) + ux * LEAVE_PUSH);
				Wr<float>(s.x + o + 8, Rd<float>(s.x + o + 8) + uz * LEAVE_PUSH);
			}
		}
		// arrival along the fight axis (enemy -> point at the request)
		const float along = (Fx(s.y) - Fx(s.e)) * s.ux + (Fz(s.y) - Fz(s.e)) * s.uz;
		if (s.phase == Phase::Enter && s.drive && (along <= s.arrive || s.t > 240)) {
			if (IsRunId(s, Rd<uint32_t>(s.y + O.moveId))) QueueMove(s.y, s.stopId, s.stopPtr);
			s.holdX = s.driveX; s.holdZ = s.driveZ; s.holdN = HOLD;
			s.drive = false;
			if (!s.combo && s.sw.valid) { DoSwap(s.sw); SetNeutral(s.y, false); OnSwapped(t); }
			s.phase = Phase::Leave; s.t = 0;
		}
		else if (s.phase == Phase::Leave && (Dist(s.x, s.e) >= s.arrive + LEAVE_EXTRA || s.t > 240)) {
			s.phase = Phase::Done;
			OnLeft(t);
		}
	}
}

// Visibility by state: a team that isn't mid-tag shows its point and hides its partner
void FixVisibility()
{
	for (int t = 0; t < 2; ++t) {
		if (g_partnerSlot[t] < 0 || g_tr[t].active || g_comboTeam[t]) continue;
		const std::pair<int, bool> want[2] = { { TEAM_POINT[t], false }, { g_partnerSlot[t], true } };
		for (auto [slot, hidden] : want) {
			const uint64_t a = ActorOf(Entry(slot));
			if (a && IsAlive(a) && IsHidden(a) != hidden) SetHidden(a, hidden);
		}
	}
}

// Floor level (stages with floor breaks): the floor height lives at player +0x354/+0x360/+0x364 (0 at +0x300) for a
// fighter that fought on that floor; a partner left with another floor's values gets that floor's offset stacked onto
// its matrices (live 2026-11-14: Dragunov 400140 / 600140 vs 201079 for the others, one and two floors up).
// The incoming fighter is put on the GROUND of `src`'s floor, not at `src`'s height (owner, 2026-11-14: tag on the
// ground even when the point is jumping): floor fields copied; position/root Y (relative, 0 = ground) := 0; the
// absolute Y of the matrices and previous-root block := floor height + the fighter's own standing offset (hips above
// the ground, ~1050-1080, read from its own matrix before the change; 1060 if that is not sane).
void MatchFloor(uint64_t dst, uint64_t src)
{
	const float oldFloor = Rd<float>(dst + 0x354) != 0 ? Rd<float>(dst + 0x354) : Rd<float>(dst + 0x300);
	float stand = Rd<float>(dst + O.matT0 + 4) - oldFloor;
	if (!(stand > 300.f && stand < 2500.f)) stand = 1060.f;
	for (int o : { 0x300, 0x354, 0x360, 0x364 }) Wr<uint32_t>(dst + o, Rd<uint32_t>(src + o));
	const float floor = Rd<float>(src + 0x354) != 0 ? Rd<float>(src + 0x354) : Rd<float>(src + 0x300);
	Wr<float>(dst + O.pos + 4, 0.f);
	Wr<float>(dst + O.root + 4, 0.f);
	for (int o : { O.matT0, O.matT3, O.prevRoot }) Wr<float>(dst + o + 4, floor + stand);
}

// Bring an entry with its fighter loaded in as `team`'s partner (tag.py partner())
void Partner(int slot, int team, bool reinit)
{
	const int pt = TEAM_POINT[team];
	const uint64_t point = Entry(pt), enemy = Entry(1 - pt);
	const uint64_t p = GetPlayer(g_rnd, slot);
	if (!p || !point || !enemy) return;
	memcpy((void*)(p + O.mats), (void*)(point + O.mats), 0x100);
	memcpy((void*)(p + O.prevRoot), (void*)(point + O.prevRoot), 0x70);
	Wr<uint16_t>(p + O.facing, Rd<uint16_t>(point + O.facing));
	Wr<uint16_t>(p + O.facing2, Rd<uint16_t>(point + O.facing2));
	MatchFloor(p, point);
	const uint64_t keep = Rd<uint64_t>(enemy + O.targeted);
	// side BEFORE InitPlayerValues: it indexes a per-side array with it (0x141911CA7: GetStruct(0x14959A090) + 0x40 +
	// side * 0x180, +0x68 written); with the setup's side 0xFE (signed -2) it wrote two records before the array
	// (silent corruption, then an AV on an unmapped page, 2026-11-15 01:07)
	Wr<uint8_t>(p + O.side, (uint8_t)team);
	Fn<fn_init_player>(F.initPlayer)(p, enemy, 1, 0);
	Wr<uint64_t>(enemy + O.targeted, keep);       // InitPlayerValues back-links the enemy to us: keep its point
	Wr<uint8_t>(p + O.side, (uint8_t)team);
	if (!reinit) {
		Fn<fn_p_u8>(F.setActive)(p, 1);
		int s = slot; Fn<fn_p_pi>(F.activateSlotUE)(g_rnd, &s);
	}
	Wr<uint8_t>(p + O.ready, 1);
	InitCpu(p, 1);
	Wr<int32_t>(p + O.inputMode, 0);              // standing by: input cleared every frame
	float sx, sz; Behind(point, enemy, STANDBY_X, STANDBY_LANE, sx, sz);
	WritePos(p, sx, sz);
}

uint64_t FindActorOfPlayer(const std::vector<uint64_t>& actors, uint64_t p)
{
	const int s = Rd<int32_t>(p + O.slot);
	for (auto a : actors) if (Rd<int32_t>(a + O.aRenderSlot) == s) return a;
	return 0;
}

// Actor + run-in moves of the fighter in `slot` (the run moves are patched into its moveset once)
void AddToCache(int slot)
{
	const uint64_t q = Entry(slot);
	if (!q) return;
	g_actor[q] = FindActorOfPlayer(CharacterActors(), q);
	const uint64_t ms = P(q + O.moveset);
	if (!ms) return;
	MoveSet m;
	m.run = InstallRunLoop(ms);
	m.runInt = InstallRunLoopInterruptible(ms);
	m.dash = FindMoveIndex(ms, { BACKDASH[0], BACKDASH[1], BACKDASH[2] });
	m.runPtr = m.run >= 0 ? GetMove(q, (uint32_t)m.run) : 0;
	m.runIntPtr = m.runInt >= 0 ? GetMove(q, (uint32_t)m.runInt) : 0;
	m.dashPtr = m.dash >= 0 ? GetMove(q, (uint32_t)m.dash) : 0;
	m.idle = GetMove(q, 0x8001);
	g_moves[q] = m;
	Log("slot %d: player %llx fighter %d actor %llx run %d/%d dash %d", slot, q, Fighter(q), g_actor[q], m.run, m.runInt, m.dash);
}

bool BuildCache()
{
	g_actor.clear(); g_moves.clear();
	for (int s : { 0, 1, g_partnerSlot[0], g_partnerSlot[1] }) if (s >= 0) AddToCache(s);
	g_dm = FindInstanceOf("PolarisDramaManager");
	g_cm = FindInstanceOf("PolarisCharacterManager");
	g_settings = Settings();
	g_cameraManager = FindCameraManager();
	Log("drama manager %llx, character manager %llx, settings %llx, camera %llx", g_dm, g_cm, g_settings, g_cameraManager);
	return g_dm && g_cm && g_settings;
}

// ---------------------------------------------------------------------------------------------------------------
// P4 loading (fourp.py f1 steps on entry 3, loadside 3, motbin, animclass, bindskel) as a per-frame state machine
// ---------------------------------------------------------------------------------------------------------------
enum class Setup { Idle, Activate, Ids, Clear, Alloc, CpuStatus, Bind, Inactive, Service, LoadRequest, Motion,
	MotionWait, ActorWait, DressWait, SharedAnims, SharedWait, Motbin, AnimClass, BindSkel, Join, Done, Failed };
// Fighters the DLL loads itself onto an extra round slot (fourp.py P4 recipe), one job after the other:
//  - reload mode (the battle was built with 4 slots): P3 is the Practice dummy (slot 2, loaded by the game), one job:
//    P4 on slot 3 / side 3 for team B;
//  - no-reload mode (a running 3-slot match grown in place, TagT8Grow): the dummy is left as the game built it
//    (inactive), jobs: P3 on slot 3 / side 3 for team A, then P4 on slot 4 / side 4 for team B (when grown to 5).
struct LoadJob { int slot = 3, team = 1, fighter = -1; };
LoadJob g_jobs[2];
int g_jobCount = 0, g_jobIdx = 0;
Setup g_setup = Setup::Idle;
bool SetupInProgress() { return g_growPending || (g_setup != Setup::Idle && g_setup != Setup::Done && g_setup != Setup::Failed); }
int g_setupAt = 0;            // frame of the next step
int g_setupDeadline = 0;
int g_dressStable = 0;
uint64_t g_bd = 0;            // MotHead binder
// slot 3's entry and loader request made during the loading screen (ActivateSlot hook), for this round object
uint64_t g_p4RequestRound = 0;
// side -> fighter the SideFighter hook returns (sides >= 3 have no settings descriptor)
int g_sideFighter[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };

LoadJob& Job() { return g_jobs[g_jobIdx]; }

uint64_t Fnv(uint32_t v)
{
	uint64_t h = 0xCBF29CE484222325ull;
	for (int i = 0; i < 4; ++i) h = (h ^ ((v >> (8 * i)) & 0xFF)) * 0x100000001B3ull;
	return h;
}

void SetupFail(const char* why)
{
	g_setup = Setup::Failed;
	SetStatus(TagState_Error, "Tag runs, but the fighter for slot %d could not be loaded (%s)", Job().slot, why);
}

void Next(Setup s, int delayFrames = 2) { g_setup = s; g_setupAt = g_frames + delayFrames; }

int IterCount() { const uint64_t r = RoundObj(); return r ? (int)((P(r + O.rIter + 8) - P(r + O.rIter)) / 8) : 0; }

// fourp.py f1 steps on an extra entry (activate ids clear alloc cpustatus bind inactive service) + the loader
// request, one step per call. The slot index is also the loader side.
bool EntryStep(Setup step, const LoadJob& j)
{
	const int slot = j.slot;
	switch (step) {
	case Setup::Activate: {
		const uint64_t s = Settings();
		const uint64_t e = EntryAddr(slot);
		if (!s || !e) return false;
		if (IterCount() > slot) return true;        // already enlisted (a reload of a match that had it)
		uint8_t* desc = (uint8_t*)GameHeapAlloc(0x100);
		memcpy(desc, (void*)(s + O.sDesc + 2ull * O.sDescStride), O.sDescStride);   // slot 2's descriptor as template
		*(int32_t*)desc = slot; desc[4] = 0xFE; *(int32_t*)(desc + O.sDescFighter) = j.fighter;
		// ActivateSlot enlists the entry; a loop then calls GetPlayer(player+4) with no null check: write the id first
		Wr<int32_t>(e + O.eToPlayer + O.slot, slot);
		Wr<int32_t>(e + O.eToPlayer + O.slot2, slot);
		Fn<uint64_t(*)(uint64_t, void*)>(F.activateSlot)(g_rnd, desc);
		return true;
	}
	case Setup::Ids: case Setup::Clear: case Setup::Alloc: case Setup::CpuStatus: {
		const uint64_t p = GetPlayer(g_rnd, slot), q = GetPlayer2(g_rnd, slot);
		if (!p) return false;
		for (uint64_t x : { p, q }) {
			if (!x) continue;
			if (step == Setup::Ids) {
				Wr<int32_t>(x + O.slot, slot); Wr<uint8_t>(x + O.side, 0xFE);
				if (x == p) CleanStalePlayer(p, slot);
			}
			else if (step == Setup::Clear) Fn<fn_p>(F.clearMoves)(x);
			else if (step == Setup::Alloc) Fn<fn_p>(F.allocHelpers)(x);
			else { int s = slot; Fn<fn_p_pi>(F.setCpuStatus)(x, &s); }
		}
		return true;
	}
	// t8-retail_tag_bind_fighter is the body (0x14191BF90): it takes the round OBJECT (reads [rcx+8]), not the registry
	// holder the other round functions take (they dereference [rcx] first; the thunk 0x14191C210 does it for this one)
	case Setup::Bind: { int s = slot; Fn<fn_bind_fighter>(F.bindFighter)(RoundObj(), &s, j.fighter); return true; }
	case Setup::Inactive: { const uint64_t p = GetPlayer(g_rnd, slot); if (!p) return false; Fn<fn_p_u8>(F.setActive)(p, 0); return true; }
	case Setup::Service: {
		const uint64_t svc = GetStruct(G.psKey);
		int s = slot;
		const uint64_t ps = svc ? Fn<fn_p_pi>(F.psGet)(svc, &s) : 0;
		if (ps) Fn<fn_p>(F.psInit)(ps);
		return true;
	}
	case Setup::LoadRequest: {
		// loader request vt+0x98(side, 3): the per-side job loads the fighter our SideFighter hook returns for the side
		g_sideFighter[slot] = j.fighter;
		const uint64_t game = P(G.gamemode);
		const uint64_t lw = Fn<fn_p>(F.loaderWorld)(game);
		const uint64_t ld = lw ? Fn<fn_p>(F.getLoader)(lw) : 0;
		g_bd = P(P(G.motheadBinder));
		if (!ld || !g_bd) return false;
		Fn<uint64_t(*)(uint64_t, int, uint32_t)>(P(P(ld) + 0x98))(ld, slot, 3);
		return true;
	}
	default: return true;
	}
}

// During the loading screen (reload mode): the game just activated the Practice dummy (slot 2) of a 4-entry battle.
// Prepare entry 3 and start loading P4 now, so its assets load with everyone else's.
void StartP4DuringLoad(uint64_t)
{
	g_rnd = GetStruct(G.roundKey);     // (ActivateSlot's first argument is not the registry's round struct)
	const uint64_t r = RoundObj();
	if (!r || EntryCount() < 4 || g_p4RequestRound == r) return;
	const LoadJob j{ 3, 1, g_p4 };
	for (Setup s : { Setup::Activate, Setup::Ids, Setup::Clear, Setup::Alloc, Setup::CpuStatus, Setup::Bind,
		Setup::Inactive, Setup::Service, Setup::LoadRequest })
		if (!EntryStep(s, j)) { Log("P4 at load: step %d failed", (int)s); return; }
	g_p4RequestRound = r;
	Log("P4 at load: entry 3 prepared, loader request side 3 fighter %d", j.fighter);
}

void HideSideActor(int side)
{
	for (auto a : CharacterActors())
		if (Rd<int32_t>(a + O.aPlayerNumber) == side && !IsHidden(a)) SetHidden(a, true);
}

// ---------------------------------------------------------------------------------------------------------------
// Shared animations (com.anmbin, domain 0x81). The game fills the shared map once per match with the fighters of the
// match settings (binder vt+0x20 0x1458F7340 -> PushMap, Populate 0x145C58770 keeps rows whose fighter bits match):
// a fighter added mid-match has no rows there, relocate writes move+0x120 = -1 and those moves end at once (Dragunov
// as an extra fighter: 36 moves incl. d,d/f,f, d/f+2, raw 4; Paul 64-94; RE session 2026-11-15, findings log). The
// missing rows are inserted into the live map the way Populate does, then binder vt+0x90 (CommonReady) is polled.
// ---------------------------------------------------------------------------------------------------------------
// MSVC unordered_map<u32, T> lookup as the game's readers do it (bucket high end, then prev links)
uint64_t UmapFind(uint64_t head, uint64_t buckets, uint64_t mask, uint32_t key)
{
	if (!head || !buckets) return 0;
	const uint64_t b = buckets + (Fnv(key) & mask) * 16, first = P(b);
	uint64_t n = P(b + 8);
	for (int i = 0; i < 16384 && n && n != head; ++i) {
		if (Rd<uint32_t>(n + 0x10) == key) return n;
		if (n == first) return 0;
		n = P(n + 8);
	}
	return 0;
}
// vec[0] map of the anim container's domain (container = *(binder + 0x10), game_addresses.ini anim_container
// "+0x9BD9DE8, 0, 0x10, 0": list +8, buckets +0x18, mask +0x30)
uint64_t AnimDomainMap(uint32_t domain)
{
	if (!g_bd) g_bd = P(P(G.motheadBinder));
	if (!g_bd) return 0;
	const uint64_t c = P(g_bd + 0x10);
	if (!c) return 0;
	const uint64_t node = UmapFind(P(c + 8), P(c + 0x18), Rd<uint64_t>(c + 0x30), domain);
	if (!node) return 0;
	const uint64_t begin = P(node + 0x18), end = P(node + 0x20);
	return begin && end > begin ? P(begin) : 0;
}
// Number of maps in the anim container's domain vector (0 if the domain is absent)
int AnimDomainMapCount(uint32_t domain)
{
	if (!g_bd) g_bd = P(P(G.motheadBinder));
	const uint64_t c = g_bd ? P(g_bd + 0x10) : 0;
	if (!c) return 0;
	const uint64_t node = UmapFind(P(c + 8), P(c + 0x18), Rd<uint64_t>(c + 0x30), domain);
	if (!node) return 0;
	const uint64_t begin = P(node + 0x18), end = P(node + 0x20);
	return end > begin ? (int)((end - begin) / 8) : 0;
}
// Insert node {key id, value ref} into the map's unordered_map (map+8: max_load float +8, list head +0x10, size +0x18,
// buckets {lo, hi} +0x20, mask +0x38, bucket count +0x40), keeping each bucket contiguous (Populate's inlined insert)
void UmapInsert(uint64_t map, uint64_t node, uint32_t id)
{
	const uint64_t head = P(map + 0x10), buckets = P(map + 0x20), mask = Rd<uint64_t>(map + 0x38);
	const uint64_t b = buckets + (Fnv(id) & mask) * 16;
	const uint64_t lo = P(b), hi = P(b + 8);
	const uint64_t where = lo == head ? head : P(hi);           // empty bucket: list end; else after its last node
	const uint64_t prev = P(where + 8);
	Wr<uint64_t>(node + 0, where);
	Wr<uint64_t>(node + 8, prev);
	Wr<uint64_t>(prev + 0, node);
	Wr<uint64_t>(where + 8, node);
	Wr<uint64_t>(map + 0x18, Rd<uint64_t>(map + 0x18) + 1);
	if (lo == head) { Wr<uint64_t>(b, node); Wr<uint64_t>(b + 8, node); }
	else Wr<uint64_t>(b + 8, node);
}
// Rows of com.anmbin tagged for `fighter` and missing from the live shared map get a ResourceRef and a map node.
// Returns the number added, or < 0 on error. `present`: rows of the fighter already in the map.
int AddSharedAnims(int fighter, int& present)
{
	present = 0;
	if (fighter < 0 || fighter >= 256 || !F.animMapContains || !G.animRefBind || !G.umapRehash || !G.vtAnimRef || !G.vtAnimFactory) return -1;
	const uint64_t map = AnimDomainMap(0x81);
	if (!map) return -2;
	const uint64_t res = P(P(map) + 8);
	const uint64_t data = res ? P(P(res + 0x118)) : 0;
	if (!data) return -3;
	std::vector<uint64_t> rows;
	std::vector<uint32_t> ids;
	for (int cat = 0; cat < 6; ++cat) {
		const uint32_t n = Rd<uint32_t>(data + 4 + 4 * cat);
		const uint64_t pool = P(data + 0x38 + 8 * cat);
		if (!pool || n > 200000) continue;
		for (uint32_t i = 0; i < n; ++i) {
			const uint64_t row = pool + (uint64_t)i * 0x38;
			if (!((Rd<uint32_t>(row + 0x18 + 4 * (fighter >> 5)) >> (fighter & 31)) & 1)) continue;
			if (!P(row + 8) && !P(row + 0x10)) continue;
			const uint32_t id = Rd<uint32_t>(row);
			if (Fn<uint64_t(*)(uint64_t, uint32_t)>(F.animMapContains)(map, id) & 0xFF) { ++present; continue; }
			if (std::find(ids.begin(), ids.end(), id) != ids.end()) continue;
			rows.push_back(row); ids.push_back(id);
		}
	}
	if (rows.empty()) return 0;
	// grow the buckets once for the batch (Populate's rule: x8 below 512 buckets; Rehash rounds up to 2^n)
	const float maxLoad = Rd<float>(map + 8);
	const uint64_t size = Rd<uint64_t>(map + 0x18), bucketCount = Rd<uint64_t>(map + 0x40);
	const uint64_t needed = (uint64_t)std::ceil((double)(size + rows.size()) / (maxLoad > 0 ? maxLoad : 1.0));
	if (needed > bucketCount)
		Fn<void(*)(uint64_t, uint64_t)>(G.umapRehash)(map + 8, bucketCount < 512 ? (std::max)(needed, bucketCount * 8) : needed);
	auto alloc = Fn<uint64_t(*)(uint64_t)>(F.malloc);
	for (size_t k = 0; k < rows.size(); ++k) {
		const uint64_t ref = alloc(0x20), impl = alloc(0x40), node = alloc(0x20);
		if (!ref || !impl || !node) return -4;
		memset((void*)ref, 0, 0x20); Wr<uint64_t>(ref, G.vtAnimRef);
		memset((void*)impl, 0, 0x40); Wr<uint64_t>(impl, G.vtAnimFactory); memcpy((void*)(impl + 8), (void*)rows[k], 0x38);
		// by-value std::function (0x40 bytes, impl pointer at +0x38): the callee destroys its argument
		alignas(16) uint8_t fn[0x40] = {};
		*(uint64_t*)(fn + 0x38) = impl;
		Fn<uint64_t(*)(uint64_t, void*, uint64_t)>(G.animRefBind)(ref, fn, 1);
		memset((void*)node, 0, 0x20);
		Wr<uint32_t>(node + 0x10, ids[k]);
		Wr<uint64_t>(node + 0x18, ref);
		UmapInsert(map, node, ids[k]);
	}
	return (int)rows.size();
}

void SetupTick()
{
	if (g_setup == Setup::Idle || g_setup == Setup::Done || g_setup == Setup::Failed) return;
	if (g_frames < g_setupAt) return;
	const LoadJob& j = Job();
	const int side = j.slot;
	switch (g_setup) {
	case Setup::Activate: case Setup::Ids: case Setup::Clear: case Setup::Alloc: case Setup::CpuStatus:
	case Setup::Bind: case Setup::Inactive: case Setup::Service: case Setup::LoadRequest:
		if (!EntryStep(g_setup, j)) { SetupFail("entry setup"); return; }
		if (g_setup == Setup::LoadRequest) { Log("slot %d: loader request side %d fighter %d", j.slot, side, j.fighter); Next(Setup::Motion, 60); }
		else Next((Setup)((int)g_setup + 1), 10);
		break;
	case Setup::Motion: {
		g_sideFighter[side] = j.fighter;
		if (!g_bd) g_bd = P(P(G.motheadBinder));
		// the MotHead store refuses any kind (0 moveset, 1 fighter data, 2 side descriptor, 3) the side still holds
		// (0x145C7C000 -> check 0x145C76AD0): release the side's whole node (all 4 holders) with the game's own
		// 0x145C7A800(store, side), round-free (RE session 2026-11-15). Only destroying the kind-0 holder kept a
		// reused side's kind-1/2 data from the previous match (the previous fighter's, if the side changed fighter).
		const uint64_t st = P(g_bd + 8);
		if (st && F.motheadStoreRelease) Fn<uint64_t(*)(uint64_t, uint32_t)>(F.motheadStoreRelease)(st, (uint32_t)side);
		else if (st) {
			const uint64_t b = P(st + 0x20) + (Rd<uint64_t>(st + 0x38) & Fnv((uint32_t)side)) * 0x10;
			uint64_t e = P(b + 8);
			for (int i = 0; i < 64 && e && Rd<uint32_t>(e + 0x10) != (uint32_t)side; ++i) e = P(e + 8);
			if (e && Rd<uint32_t>(e + 0x10) == (uint32_t)side) {
				const uint64_t h = P(e + 0x18);
				if (h) { Wr<uint64_t>(e + 0x18, 0); Fn<uint64_t(*)(uint64_t, uint32_t)>(P(P(h)))(h, 1); }
			}
		}
		Fn<fn_p_u32_u32>(F.motionRequest)(g_bd, (uint32_t)side, (uint32_t)j.fighter);
		g_setupDeadline = g_frames + 60 * 60;
		Next(Setup::MotionWait, 5);
		break;
	}
	case Setup::MotionWait:
		if (Fn<fn_p_u32>(F.motionReady)(g_bd, (uint32_t)side) & 0xFF) { Log("slot %d: motion ready", j.slot); g_setupDeadline = g_frames + 60 * 60; Next(Setup::ActorWait, 1); }
		else if (g_frames > g_setupDeadline) SetupFail("motion data not ready after 60 s");
		else Next(Setup::MotionWait, 5);
		break;
	case Setup::ActorWait: {
		bool found = false;
		for (auto a : CharacterActors()) if (Rd<int32_t>(a + O.aPlayerNumber) == side && Rd<int32_t>(a + O.aFighter) == j.fighter) found = true;
		if (found) { HideSideActor(side); Log("slot %d: actor spawned", j.slot); g_dressStable = 0; g_setupDeadline = g_frames + 25 * 60; Next(Setup::DressWait, 1); }
		else if (g_frames > g_setupDeadline) SetupFail("no actor after 60 s");
		else Next(Setup::ActorWait, 10);
		break;
	}
	case Setup::DressWait: {
		// the costume coroutine ends with the merged mesh on MasterMesh: wait for it (stable for 20 frames)
		HideSideActor(side);
		bool dressed = false;
		for (auto a : CharacterActors())
			if (Rd<int32_t>(a + O.aPlayerNumber) == side) {
				const uint64_t mm = P(a + O.aMasterMesh);
				dressed = mm && g_skinnedAssetOff > 0 && P(mm + g_skinnedAssetOff);
			}
		g_dressStable = dressed ? g_dressStable + 5 : 0;
		if (g_dressStable >= 20 || g_frames > g_setupDeadline || g_skinnedAssetOff <= 0) {
			Log("slot %d: costume %s", j.slot, g_dressStable >= 20 ? "ready" : "wait timed out");
			Next(Setup::SharedAnims, 1);
		}
		else Next(Setup::DressWait, 5);
		break;
	}
	case Setup::SharedAnims: {
		int present = 0;
		const int added = AddSharedAnims(j.fighter, present);
		if (added < 0) Log("slot %d: shared animations not extended (error %d); its shared moves will end at once", j.slot, added);
		else Log("slot %d: %d shared animation rows added (%d already there)", j.slot, added, present);
		g_setupDeadline = g_frames + 30 * 60;
		Next(Setup::SharedWait, 2);
		break;
	}
	case Setup::SharedWait:
		if (Fn<uint64_t(*)(uint64_t)>(F.binderCommonReady)(g_bd) & 0xFF) { Log("slot %d: shared animations ready", j.slot); Next(Setup::Motbin, 1); }
		else if (g_frames > g_setupDeadline) { Log("slot %d: shared animations not ready after 30 s, relocating anyway", j.slot); Next(Setup::Motbin, 1); }
		else Next(Setup::SharedWait, 5);
		break;
	case Setup::Motbin: {
		const uint64_t p = GetPlayer(g_rnd, j.slot);
		const uint64_t store = P(g_bd + 8);
		const uint64_t data = Fn<fn_getcopy>(F.motheadGetCopy)(store, 0, (uint32_t)side);
		if (!data || !p) { SetupFail("no moveset copy"); return; }
		Fn<fn_relocate>(F.movesetRelocate)(j.fighter, data, 0);
		Fn<fn_init_mvp>(F.initMovesetParent)(p, j.fighter, data, 0);
		Log("slot %d: moveset bound (%llx)", j.slot, P(p + O.moveset));
		Next(Setup::AnimClass, 5);
		break;
	}
	case Setup::AnimClass: {
		// the mid-match actor gets its merged mesh but no anim blueprint (T-pose): give it another actor's
		uint64_t mine = 0, donor = 0;
		for (auto a : CharacterActors()) {
			const uint64_t mm = P(a + O.aMasterMesh);
			if (Rd<int32_t>(a + O.aPlayerNumber) == side) mine = a;
			else if (mm && g_animClassOff > 0 && P(mm + g_animClassOff)) donor = P(mm + g_animClassOff);
		}
		const uint64_t mm = mine ? P(mine + O.aMasterMesh) : 0;
		if (mm && donor && g_animClassOff > 0 && !P(mm + g_animClassOff)) {
			uint8_t buf[64] = {};
			*(uint64_t*)(buf + g_animClassParamOff) = donor;
			CallUFunc(mm, g_setAnimClass, buf);
			Log("slot %d: anim class set", j.slot);
		}
		Next(Setup::BindSkel, 5);
		break;
	}
	case Setup::BindSkel:
		// the game's per-side skeleton binder (MotHead binder vt+0x58) for the side = slot
		Fn<uint64_t(*)(uint64_t, uint32_t)>(P(P(g_bd) + 0x58))(g_bd, (uint32_t)side);
		Next(Setup::Join, 10);
		break;
	case Setup::Join: {
		// the fighter becomes its team's partner
		Partner(j.slot, j.team, false);
		AddToCache(j.slot);
		SetHidden(ActorOf(Entry(j.slot)), true);
		g_partnerSlot[j.team] = j.slot;
		g_snap = SNAP;
		if (g_jobIdx + 1 < g_jobCount) { ++g_jobIdx; Next(Setup::Activate, 10); }
		else g_setup = Setup::Done;
		const bool a = g_partnerSlot[0] >= 0, b = g_partnerSlot[1] >= 0;
		SetStatus(TagState_Running, "Tag running: %s%s%s, Ki Charge tags%s", a ? "P1 + P3" : "", a && b ? " and " : "",
			b ? "P2 + P4" : "", g_setup == Setup::Done ? "" : " (loading the next fighter...)");
		break;
	}
	default: break;
	}
}

// In-place grow to `n` slots (TagT8Grow); 4 for now, 5 once the routine takes a target count
bool GrowTo(int n, std::string& err, TagT8Grow::GrowReport& rep)
{
	return TagT8Grow::GrowBattleTo(g_grow, n, err, &rep);
}

// Where an exception happened: module + offset (game RVA or this DLL's)
void LogException(const char* where, EXCEPTION_POINTERS* ep)
{
	const uint64_t a = (uint64_t)ep->ExceptionRecord->ExceptionAddress;
	HMODULE m = nullptr;
	char name[MAX_PATH] = "?";
	if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)a, &m))
		GetModuleFileNameA(m, name, MAX_PATH);
	const char* base = strrchr(name, '\\');
	Log("%s: exception %08X at %s+0x%llX (read/write %llu at %llX)", where, ep->ExceptionRecord->ExceptionCode,
		base ? base + 1 : name, a - (uint64_t)m, ep->ExceptionRecord->NumberParameters > 1 ? ep->ExceptionRecord->ExceptionInformation[0] : 0,
		ep->ExceptionRecord->NumberParameters > 1 ? ep->ExceptionRecord->ExceptionInformation[1] : 0);
}
// The extra round entries (slots 3/4) are not rebuilt by the game between matches: their player struct keeps the last
// match's state. A P4 that went NaN in one match came back NaN-poisoned in the next (live 2026-11-14 23:55: 228 NaN
// floats while the entry was still inactive; the game's simulation then keeps the NaN alive through the bone/hitbox
// blocks and the camera's angle-wrap loop 0x141811752 spins forever). Copying a game-built player's value into every
// NaN/inf float (where that player's is finite) leaves it clean for good (live: 0 back after 2 s).
void CleanStalePlayer(uint64_t p, int slot)
{
	uint64_t donor = 0;
	for (int s : { 2, 1, 0 }) if (s != slot) { donor = GetPlayer(g_rnd, s); if (donor) break; }
	if (!donor) return;
	auto bad = [](uint32_t u) { return (u & 0x7F800000u) == 0x7F800000u; };
	int n = 0;
	for (int o = 0; o < 0x4000; o += 4) {
		const uint32_t v = Rd<uint32_t>(p + o), d = Rd<uint32_t>(donor + o);
		if (bad(v) && !bad(d)) { Wr<uint32_t>(p + o, d); ++n; }
	}
	if (n) Log("slot %d: %d stale NaN/inf floats cleaned (from the slot's previous match)", slot, n);
}

bool ReadQword(uint64_t a, uint64_t& v) { __try { v = *(uint64_t*)a; return true; } __except (EXCEPTION_EXECUTE_HANDLER) { return false; } }
// Return addresses on the faulting thread's stack that sit in this DLL or the game (a cheap backtrace for the log)
void LogStack(EXCEPTION_POINTERS* ep)
{
	// real unwind through the x64 unwind tables (frames without unwind info: leaf, pop the return address)
	CONTEXT c = *ep->ContextRecord;
	std::string line;
	for (int i = 0; i < 18 && c.Rip; ++i) {
		HMODULE m = nullptr;
		char name[MAX_PATH] = "?";
		if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)c.Rip, &m))
			GetModuleFileNameA(m, name, MAX_PATH);
		const char* b = strrchr(name, '\\'); b = b ? b + 1 : name;
		char w[96];
		if (!_strnicmp(b, "Polaris", 7)) snprintf(w, sizeof(w), " 0x%llX", c.Rip - (uint64_t)m + 0x140000000ull);
		else snprintf(w, sizeof(w), " %s+0x%llX", !_strnicmp(b, "TKMovesetLoader", 15) ? "dll" : b, c.Rip - (uint64_t)m);
		line += w;
		DWORD64 imageBase = 0;
		PRUNTIME_FUNCTION rf = RtlLookupFunctionEntry(c.Rip, &imageBase, nullptr);
		if (!rf) { uint64_t ret = 0; if (!ReadQword(c.Rsp, ret)) break; c.Rip = ret; c.Rsp += 8; continue; }
		PVOID handlerData = nullptr; DWORD64 establisher = 0;
		RtlVirtualUnwind(UNW_FLAG_NHANDLER, imageBase, c.Rip, rf, &c, &handlerData, &establisher, nullptr);
	}
	Log("  stack:%s", line.c_str());
}
int g_setupErrors = 0;
int FilterSetup(EXCEPTION_POINTERS* ep)
{
	if (++g_setupErrors <= 3) { char w[64]; snprintf(w, sizeof(w), "slot setup step %d", (int)g_setup); LogException(w, ep); LogStack(ep); }
	return EXCEPTION_EXECUTE_HANDLER;
}
int SafeSetupTick()
{
	__try { SetupTick(); return 0; }
	__except (FilterSetup(GetExceptionInformation())) { return 1; }
}

// A loader started in a layout a previous loader left swapped (the partner's physical entry sits in the point
// slot): record it as swapped so a Practice reset / match load puts it back
void MarkIfSwapped(int team)
{
	const int b = g_partnerSlot[team];
	if (b < 0 || b >= EntryCount()) return;
	if (Entry(TEAM_POINT[team]) == EntryAddr(b) + O.eToPlayer) {
		g_swapped[team] = true;
		g_revert[team] = MakeSwap(team);
		Log("team %c found swapped (left by a previous loader): undone at the next reset or load", 'A' + team);
	}
}

// An extra slot already built in this match (a previous loader did it, then the DLL was swapped): active, with this
// fighter and a moveset. Re-running the pipeline on it clears its moves and destroys its moveset holder (fatal error,
// 21:58). Checked on the PHYSICAL entry (the slot table may hold the other team member after a swap).
bool SlotBuilt(int slot, int fighter, int entries)
{
	const uint64_t e = slot < entries ? EntryAddr(slot) : 0;
	const uint64_t q = e ? e + O.eToPlayer : 0;
	return q && Rd<uint8_t>(e) == 1 && Rd<uint8_t>(q + 8) == 1 && P(q + O.moveset) && Fighter(q) == fighter;
}

void Rejoin(int slot, int team)
{
	Partner(slot, team, true);
	AddToCache(slot);
	SetHidden(ActorOf(Entry(slot)), true);
	g_partnerSlot[team] = slot;
	MarkIfSwapped(team);
}

bool StartMatch(int entries)
{
	g_partnerSlot[0] = g_partnerSlot[1] = -1;
	g_jobCount = g_jobIdx = 0;
	if (!BuildCache()) { SetStatus(TagState_Error, "Tag: managers not found"); return false; }
	g_snap = SNAP;
	g_running = true;
	const uint64_t dummy = GetPlayer(g_rnd, 2);
	const bool dummyIsP3 = dummy && Fighter(dummy) == g_p3 && P(dummy + O.moveset);
	if (kReloadMode && (entries >= 4 || dummyIsP3)) {
		// reload mode: P3 = the Practice dummy (its fighter was swapped at SetGamemode / scene build)
		const bool active = dummy && Rd<uint8_t>(dummy - O.eToPlayer) == 1 && Rd<uint8_t>(dummy + 8) == 1;
		Partner(2, 0, active);
		AddToCache(2);
		SetHidden(ActorOf(Entry(2)), true);
		g_partnerSlot[0] = 2;
		MarkIfSwapped(0);
		if (g_p4 >= 0 && entries >= 4) {
			if (SlotBuilt(3, g_p4, entries)) { Rejoin(3, 1); g_setup = Setup::Done; }
			else {
				g_jobs[0] = LoadJob{ 3, 1, g_p4 }; g_jobCount = 1;
				// started at load (ActivateSlot hook): straight to the moveset request; else every entry step here
				Next(g_p4RequestRound == RoundObj() ? Setup::Motion : Setup::Activate, 1);
			}
		}
		else if (g_p4 >= 0 && g_growReady) { g_jobs[0] = LoadJob{ 3, 1, g_p4 }; g_jobCount = 1; g_growTarget = 4; g_growPending = true; }
	}
	else if (g_growReady) {
		// no-reload mode: the dummy stays as the game built it (inactive); P3 (and P4) on new slots
		g_jobs[0] = LoadJob{ 3, 0, g_p3 }; g_jobCount = 1;
		if (g_p4 >= 0 && g_growMax >= 5) { g_jobs[1] = LoadJob{ 4, 1, g_p4 }; g_jobCount = 2; }
		g_growTarget = 3 + g_jobCount;
		g_growPending = true;
	}
	if (g_growPending) SetStatus(TagState_Running, "Tag: adding %d slot(s) to this match...", g_growTarget - entries);
	else if (g_jobCount) SetStatus(TagState_Running, "Tag running for P1 + P3 (Ki Charge tags); P4 loading...");
	else if (g_partnerSlot[0] >= 0 && g_partnerSlot[1] >= 0) SetStatus(TagState_Running, "Tag running: P1 + P3 and P2 + P4, Ki Charge tags");
	else if (g_partnerSlot[0] >= 0) SetStatus(TagState_Running, "Tag running for P1 + P3 (Ki Charge tags)");
	else SetStatus(TagState_Error, "Tag: this match can't take more fighters (in-place grow unavailable); a Quick Select adds them");
	return true;
}


void ResetMatchState()
{
	for (int t = 0; t < 2; ++t) { Finish(t); g_tq[t] = Queued{}; g_revert[t] = SwapInfo{}; g_swapped[t] = false; g_comboTeam[t] = false;
		g_heldSince[t] = -1; g_pendingTag[t] = false; }
	memset(g_neutral, 0, sizeof(g_neutral));
	g_actor.clear(); g_moves.clear(); g_beingHit.clear();
	g_running = false; g_setup = Setup::Idle; g_reinitAt = -1; g_matchRound = 0; g_partnerSlot[0] = g_partnerSlot[1] = -1; g_jobCount = g_jobIdx = 0; g_growPending = false;
	g_cameraManager = 0; g_dm = g_cm = 0;
}

// ---------------------------------------------------------------------------------------------------------------
// Frame tick (end of the battle frame function, game thread)
// ---------------------------------------------------------------------------------------------------------------
// What the cached state was built for: a match reload (character reselect, Quick Select) keeps the round object but
// changes the fighters, their movesets and actors
struct MatchKey { uint64_t round = 0, ms[3] = {}; int fighter[3] = {}; };
MatchKey g_matchKey;
MatchKey CurrentKey(uint64_t r)
{
	MatchKey k; k.round = r;
	for (int s = 0; s < 3; ++s) {
		const uint64_t e = EntryAddr(s);
		const uint64_t p = e ? e + O.eToPlayer : 0;
		if (p) { k.ms[s] = P(p + O.moveset); k.fighter[s] = Fighter(p); }
	}
	return k;
}
// Round and fighters only, not the moveset pointers: with the same fighter on P1 and P3 a Practice reset gave entry 0
// another moveset copy, which read as a reload: Tag state reset without undoing the swaps, then P3/P4 set up again on
// live slots (a costume material freed in use: LowLevelFatalError, 2026-11-15 02:51). A real reload always goes
// through LoadMatch (HookLoadMatch -> RestoreForReload); this check is only the fallback.
bool SameKey(const MatchKey& a, const MatchKey& b) { return a.round == b.round && !memcmp(a.fighter, b.fighter, sizeof(a.fighter)); }
ULONGLONG g_lastTickMs = 0;

void Tick()
{
	g_frames++;
	g_rnd = GetStruct(G.roundKey);
	const uint64_t r = RoundObj();
	if (!r) return;
	// The in-place grow resizes the battle core, so a reload after Tag builds a 5-entry match by itself: entries 3/4
	// come back ACTIVE (player +8 = 1, sides 0/1) with no fighter data, the game simulates them empty, and their
	// state goes NaN for good (slot 4 after a Quick Select, 2026-11-15 01:22). While Tag is not running, keep every
	// extra player inactive, as the setup pipeline leaves them (SetActive 0, side 0xFE).
	if (!g_running) {
		const int n = EntryCount();
		for (int s = 3; s < n && s < 8; ++s) {
			// by storage index: the reloaded match's slot tables do not map these entries (GetPlayer returned 0)
			const uint64_t e = EntryAddr(s);
			const uint64_t p = e ? e + O.eToPlayer : 0;
			if (!p || Rd<uint8_t>(p + 8) != 1) continue;
			Fn<fn_p_u8>(F.setActive)(p, 0);
			Wr<uint8_t>(p - O.eToPlayer, 0);
			Wr<uint8_t>(p + O.side, 0xFE);
			Log("slot %d: extra entry left active by the game (fighter %d): made inactive", s, Fighter(p));
		}
	}
	const ULONGLONG now = GetTickCount64();
	const bool gap = g_lastTickMs && now - g_lastTickMs > 500;     // no battle frame for 0.5 s: loading screen
	g_lastTickMs = now;
	if (g_matchRound && (gap || g_frames % 30 == 0) && !SameKey(CurrentKey(r), g_matchKey)) {
		Log("match reloaded (fighters %d/%d/%d): tag state reset", Fighter(EntryAddr(0) + O.eToPlayer), Fighter(EntryAddr(1) + O.eToPlayer), Fighter(EntryAddr(2) + O.eToPlayer));
		ResetMatchState();
		if (g_enabled) SetStatus(TagState_WaitingForPractice, "Tag on: setting up again after the reload");
		else SetStatus(TagState_Off, "Tag: off");
		return;
	}
	if (g_matchRound && r != g_matchRound) {           // another match / stage reload: everything cached is stale
		Log("new round object: tag state reset");
		ResetMatchState();
		if (g_enabled) SetStatus(TagState_WaitingForPractice, "Tag on: enter Practice to load P3/P4");
		else SetStatus(TagState_Off, "Tag: off");
	}
	if (!g_matchRound) {
		if (!g_enabled) return;
		// a Practice battle: slot 2 is the Practice dummy (P3); slot 3 exists only if the battle was built for 4
		const int n = EntryCount();
		if (n < 3 || !Entry(0) || !Entry(1) || !P(Entry(0) + O.moveset) || !P(Entry(1) + O.moveset)) return;
		const uint64_t dummy = GetPlayer(g_rnd, 2);
		if (!dummy || !P(dummy + O.moveset)) return;
		Log("Practice battle: %d entries, slot 2 fighter %d (P3 wanted %d), P4 %s", n, Fighter(dummy), g_p3.load(),
			g_p4RequestRound == r ? "requested at load" : "not requested yet");
		if (!StartMatch(n)) return;
		// set up only once StartMatch succeeded (an exception there leaves it unset: retried next frame)
		g_matchRound = r;
		g_matchKey = CurrentKey(r);
		return;
	}
	if (!g_running) return;
	if (g_growPending) {
		// once per match, between two battle frames (end of the frame hook, game thread)
		g_growPending = false;
		TagT8Grow::GrowReport rep;
		std::string err;
		if (GrowTo(g_growTarget, err, rep) && EntryCount() >= g_growTarget) {
			Log("battle grown to %d slots in place: %s", g_growTarget, TagT8Grow::Describe(rep).c_str());
			SetStatus(TagState_Running, "Tag running; loading the extra fighter(s)...");
			Next(Setup::Activate, 1);
		}
		else {
			Log("grow refused: %s (entries %d)", err.c_str(), EntryCount());
			SetStatus(err.rfind("FAULT", 0) == 0 ? TagState_Error : TagState_Running,
				"Tag running for P1 + P3. P4: growing this match to 4 slots failed (%s); a Quick Select adds the slot.", err.c_str());
		}
	}
	if (SafeSetupTick()) SetupFail("exception, see tag.log");

	const uint64_t p0 = Entry(0), p1 = Entry(1);
	if (!p0 || !p1) return;
	for (int s : { 0, 1, g_partnerSlot[0], g_partnerSlot[1] }) if (s >= 0) UpdateHit(Entry(s));
	for (int t = 0; t < 2; ++t) if (g_kiRequest[t].exchange(false)) RequestTag(t);
	CheckQueue();
	for (int t = 0; t < 2; ++t) if (g_pendingTag[t]) { g_pendingTag[t] = false; BeginTag(t); }
	if (!g_tr[0].active && g_partnerSlot[0] >= 0) Follow(p0, Entry(g_partnerSlot[0]), p1);
	if (!g_tr[1].active && g_partnerSlot[1] >= 0) Follow(p1, Entry(g_partnerSlot[1]), p0);
	if (g_snap > 0) g_snap--;
	// NaN guard (live 2026-11-14 23:28: a partner's yaw/matrices went NaN; once a point, the camera's angle-wrap loop
	// 0x141811752 spun forever). Every frame, any NaN placement float of a Tag fighter is replaced from a sane one.
	{
		const uint64_t all[4] = { p0, p1, g_partnerSlot[0] >= 0 ? Entry(g_partnerSlot[0]) : 0, g_partnerSlot[1] >= 0 ? Entry(g_partnerSlot[1]) : 0 };
		const uint64_t donor[4] = { p1, p0, p0, p1 };
		static int repairs = 0;
		for (int i = 0; i < 4; ++i) if (all[i] && donor[i] && !SaneState(all[i]) && SaneState(donor[i])) {
			RepairState(all[i], donor[i]);
			if (repairs++ % 600 == 0) Log("NaN guard: repaired fighter %d (%d repairs so far)", Fighter(all[i]), repairs);
		}
	}
	UpdateTransitions();
	if (g_reinitAt >= 0 && g_frames >= g_reinitAt) {
		// Practice restart: the game re-placed and re-inited the two points; re-init both partners
		g_reinitAt = -1;
		g_muteRestart = true;
		for (int t = 0; t < 2; ++t) if (g_partnerSlot[t] >= 0) { Partner(g_partnerSlot[t], t, true); SetHidden(ActorOf(Entry(g_partnerSlot[t])), true); }
		g_muteRestart = false;
		g_restartFrame = g_frames;
		for (int s : { 0, 1 }) SetHidden(ActorOf(Entry(s)), false);
		g_snap = SNAP;
		Log("Practice restart: partners re-initialised");
	}
	if (g_frames % 15 == 0) FixVisibility();
}

int g_tickExc = 0;
int FilterTick(EXCEPTION_POINTERS* ep) { if (++g_tickExc <= 5) LogException("frame tick", ep); return EXCEPTION_EXECUTE_HANDLER; }
int SafeTick()
{
	__try { Tick(); return 0; }
	__except (FilterTick(GetExceptionInformation())) { return 1; }
}

// ---------------------------------------------------------------------------------------------------------------
// Hooks
// ---------------------------------------------------------------------------------------------------------------
using fn4 = uint64_t(*)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
using fn6 = fn4;
// Trampolines, looked up once per hook (hooks are installed before any of them can run, and never removed while
// the game runs)
uint64_t T(const char* name)
{
	thread_local std::map<const char*, uint64_t> cache;     // keyed by the literal's address
	auto it = cache.find(name);
	if (it != cache.end()) return it->second;
	const uint64_t t = (uint64_t)g_loader->CastTrampoline<void*>(name);
	if (t) cache[name] = t;
	return t;
}

int g_tickErrors = 0;
uint64_t HookFrame(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e8, uint64_t f8, uint64_t g8, uint64_t h8)
{
	HookGuard guard;
	const uint64_t ret = ((fn4)T("TAG_Frame"))(a, b, c, d, e8, f8, g8, h8);
	if (SafeTick() && ++g_tickErrors <= 5) Log("frame tick: exception caught (%d)", g_tickErrors);
	return ret;
}

bool PracticeSettings()
{
	// Practice: the match settings hold a dummy descriptor (fighter 116, or P3 once replaced)
	__try {
		const uint64_t s = Settings();
		if (!s) return false;
		const uint32_t f = Rd<uint32_t>(s + O.sDummyFighter);
		if (f == 116 && g_p3 >= 0) { Wr<uint32_t>(s + O.sDummyFighter, (uint32_t)g_p3.load()); Log("Practice dummy -> fighter %d (P3, at scene build)", g_p3.load()); }
		return f == 116 || (int)f == g_p3;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

uint64_t HookSceneCtor(uint64_t self, uint64_t countPtr, uint64_t c, uint64_t d, uint64_t e, uint64_t f, uint64_t g8, uint64_t h8)
{
	HookGuard guard;
	// Every scene while Tag is on, as fourp.py reload4 did (menu scenes 2, Practice 3 -> 4; healthy): Practice's
	// scene is built when the mode is entered, BEFORE SetGamemode marks the settings as Practice
	if (kReloadMode && g_enabled) PracticeSettings();          // P3 into the dummy descriptor if SetGamemode ran before the hook
	if (kReloadMode && g_enabled && countPtr) {
		const int n = *(int*)countPtr;
		if (n >= 2 && n < 4) { *(int*)countPtr = 4; Log("battle scene: %d -> 4 entries", n); }
	}
	return ((fn6)T("TAG_SceneCtor"))(self, countPtr, c, d, e, f, g8, h8);
}

uint64_t HookSetGamemode(uint64_t settings, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f, uint64_t g8, uint64_t h8)
{
	++g_inHook;
	const uint64_t ret = ((fn6)T("TAG_SetGamemode"))(settings, b, c, d, e, f, g8, h8);
	__try {
		const uint64_t dummy = settings + O.sDummyFighter;
		g_practiceDummy = Rd<uint32_t>(dummy) == 116;      // Practice builds a dummy descriptor {2, 0xFE, 116}
		if (kReloadMode && g_enabled && g_practiceDummy && g_p3 >= 0) { Wr<uint32_t>(dummy, (uint32_t)g_p3.load()); Log("Practice dummy -> fighter %d (P3)", g_p3.load()); }
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {}
	--g_inHook;
	return ret;
}

// Practice reload (Quick Select, character reselect) with 4 slots: the scene keeps its battle core, sized for 3 when
// Practice was entered. When LoadMatch calls SetupBattle (return site t8-retail_tag_loadmatch_setup_ret), build a
// 4-slot core and put it in the scene first; the reload then builds 4 round entries (RE session, confirmed live
// 2026-11-14 21:34). The old core is kept allocated: something may still read it.
void SwapCoreTo(uint64_t scene, int n, int from)
{
	const uint64_t core = P(scene + 0x48);
	if (!core) return;
	const uint64_t fresh = Fn<uint64_t(*)(uint64_t)>(F.malloc)(0x18);
	if (!fresh) return;
	Fn<uint64_t(*)(uint64_t, int*)>(F.battleCoreCtor)(fresh, &n);
	Wr<uint64_t>(scene + 0x48, fresh);
	Fn<uint64_t(*)(uint64_t)>(F.setBattleRegistry)(fresh);
	Log("Practice reload: battle core %d -> %d slots", from, n);
}
int FilterSwap(EXCEPTION_POINTERS* ep) { LogException("core swap", ep); return EXCEPTION_EXECUTE_HANDLER; }
int SafeSwapCore(uint64_t scene, int n, int from)
{
	__try { SwapCoreTo(scene, n, from); return 0; }
	__except (FilterSwap(GetExceptionInformation())) { return 1; }
}

uint64_t HookSetupBattle(uint64_t scene, uint64_t b, uint64_t c, uint64_t d, uint64_t e8, uint64_t f8, uint64_t g8, uint64_t h8)
{
	++g_inHook;
	// reached by a tail jump from the scene's vt+0x38: the return address is LoadMatch's call site
	if ((uint64_t)_ReturnAddress() == F.loadMatchSetupRet) {
		const uint64_t core = P(scene + 0x48);
		if (kReloadMode && g_enabled && PracticeSettings() && core && Rd<int32_t>(core + 8) < 4) SafeSwapCore(scene, 4, Rd<int32_t>(core + 8));
		// A reload after an in-place grow: the game never rebuilds the core on a Quick Select (grow session, static),
		// so the next match kept 5 round entries: the extra ones came back active with sides 0/1, their state stale,
		// and the Practice reset spawned P1/P2 on a layout for more players (on the Z axis, facing +-90 degrees,
		// 2026-11-15 02:41). A fresh 3-slot core gives the match the game builds by itself; Load grows it again. The
		// old core is leaked (~1.4 MB per reload): something may still read it.
		else if (!kReloadMode && EntryCount() > 3) SafeSwapCore(scene, 3, EntryCount());
	}
	const uint64_t ret = ((fn4)T("TAG_SetupBattle"))(scene, b, c, d, e8, f8, g8, h8);
	--g_inHook;
	return ret;
}

// A match load is starting (LoadMatch entry: Quick Select, character reselect, Practice entry): give the game back
// the match as it built it before it tears it down. A Quick Select with the partner left active crashed in the
// Unreal-side teardown (0x141903FC0 <- 0x1419BB5B0 <- dtor 0x142CE67E0, 2026-11-14 22:02): the Practice dummy is
// never active in a match the game built. Swaps undone, partners inactive dummies again, input and visibility back.
void RestoreForReload()
{
	if (!g_running) { ResetMatchState(); return; }
	g_rnd = GetStruct(G.roundKey);
	for (int t = 0; t < 2; ++t) {
		Finish(t);
		if (g_revert[t].valid) { DoSwap(g_revert[t]); g_revert[t] = SwapInfo{}; }
	}
	for (int t = 0; t < 2; ++t) {
		const int slot = g_partnerSlot[t];
		if (slot < 0) continue;
		const uint64_t p = GetPlayer(g_rnd, slot);
		if (!p) continue;
		Fn<fn_p_u8>(F.setActive)(p, 0);
		Wr<uint8_t>(p - O.eToPlayer, 0);
		Wr<uint8_t>(p + O.side, 0xFE);
		Wr<int32_t>(p + O.inputMode, 0);
		SetHidden(ActorOf(p), false);
	}
	for (int s : { 0, 1 }) SetHidden(ActorOf(Entry(s)), false);
	// Anim maps of the sides this loader requested (3/4): the game's ReleaseAll (binder vt+0x48) only covers sides <
	// the settings count, so they were never popped. PopMap (0x145C4F460) removes the vector's back, readers use
	// vec[0]: every match stacked more maps and vec[0] stayed the first match's one, which left that fighter NaN after
	// a reload (Paul: domain 0 held 4 maps, 2026-11-15 01:35). Every job that got as far as its motion request
	// (g_sideFighter set), joined or not.
	if (!g_bd) g_bd = P(P(G.motheadBinder));
	for (int k = 0; k < g_jobCount && k < 2 && g_bd; ++k) {
		const int slot = g_jobs[k].slot;
		if (slot < 3 || slot >= 8 || g_sideFighter[slot] < 0) continue;
		// NOT the binder's full Release(side) (vt+0x40, 0x1458F7740): it also releases the side's player
		// (0x14190FF60 -> 0x14190BBE0), whose helper objects (+0x20/+0x28/+0x30) the next match's AllocHelpers then
		// kept as dangling pointers (null vtable at +0x30, AV 0x141911226 in InitPlayerValues, 2026-11-15 01:48).
		// One Load pushes TWO maps for the side's fighter (the motion request and the loader's side request; live:
		// 1 -> 3 per Load): pop the domain down to the maps of the game's own sides (0..2) using
		// the same fighter. Our pushes came after the game's, so the back of the vector is always ours.
		const int f = g_sideFighter[slot];
		int keep = 0;
		for (int s = 0; s < 3; ++s) { const uint64_t q = GetPlayer(g_rnd, s); if (q && Fighter(q) == f && P(q + O.moveset)) ++keep; }
		int popped = 0;
		for (int i = 0; i < 16 && F.animPopMap; ++i) {
			const int n = AnimDomainMapCount((uint32_t)f);
			if (n <= keep) break;
			Fn<uint64_t(*)(uint64_t, uint32_t)>(F.animPopMap)(P(g_bd + 0x10), (uint32_t)f);
			++popped;
		}
		Log("slot %d: %d map(s) of fighter %d popped (%d kept for the game's sides)", slot, popped, f, keep);
	}
	Log("match teardown: tag undone (partners inactive again)");
	ResetMatchState();
	if (!kReloadMode && g_enabled) {
		// Tag does not survive a reload yet (the reused extra entries keep stale state; to be fixed another day):
		// switch it off; Tools > Extra > Load sets it up again in the new match
		g_enabled = false;
		SetStatus(TagState_Off, "Tag: off after the reload. Press Load again in the new match");
		return;
	}
	SetStatus(g_enabled ? TagState_WaitingForPractice : TagState_Off, g_enabled ? "Tag on: setting up again after the reload" : "Tag: off");
}
int FilterRestore(EXCEPTION_POINTERS* ep) { LogException("restore for reload", ep); return EXCEPTION_EXECUTE_HANDLER; }
int SafeRestoreForReload()
{
	__try { RestoreForReload(); return 0; }
	__except (FilterRestore(GetExceptionInformation())) { return 1; }
}

// (2026-11-15: a hook on 0x1419BB5B0 meant to restore before the "battle core destructor" was removed: grow session
// found it destroys a UObject's vector of 0x58-byte elements, never the live core (real core dtor = vt[0]
// 0x1418E55C0). The match teardown restore runs from LoadMatch, which is enough in the mid-match-only mode.)

uint64_t HookLoadMatch(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e8, uint64_t f8, uint64_t g8, uint64_t h8)
{
	// (no in-hook count: LoadMatch is a fiber that yields for seconds; the count would block unloading)
	SafeRestoreForReload();
	return ((fn4)T("TAG_LoadMatch"))(a, b, c, d, e8, f8, g8, h8);
}

// Per-player round init (0x1418F7D80(player, target, flag)): reads the current move (+0x3E0) with no null check. After
// a Practice reset in a match grown in place, the Practice tick (0x141879590) ran it on an extra slot whose move was
// cleared: crash 0x1418F8456 (2026-11-14 22:47). Guard: a player of ours with no current move gets its idle move.
void EnsureMove(uint64_t p)
{
	// any player of the round (the crashing one at 22:47 was the Practice dummy, active but not a partner: grow
	// session, from the dump)
	if (!g_running || !p || P(p + O.movePtr) || !P(p + O.moveset)) return;
	const uint64_t idle = GetMove(p, 0x8001);
	if (!idle) return;
	for (int o : { O.clearedA, O.movePtr, O.clearedC }) if (!P(p + o)) Wr<uint64_t>(p + o, idle);
	Log("round init on fighter %d with no current move: idle move given", Fighter(p));
}
int FilterEnsure(EXCEPTION_POINTERS* ep) { LogException("round init guard", ep); return EXCEPTION_EXECUTE_HANDLER; }
void SafeEnsureMove(uint64_t p)
{
	__try { EnsureMove(p); }
	__except (FilterEnsure(GetExceptionInformation())) {}
}

uint64_t HookPlayerRoundInit(uint64_t p, uint64_t b, uint64_t c, uint64_t d, uint64_t e8, uint64_t f8, uint64_t g8, uint64_t h8)
{
	HookGuard guard;
	SafeEnsureMove(p);
	return ((fn4)T("TAG_PlayerRoundInit"))(p, b, c, d, e8, f8, g8, h8);
}

int FilterLoad(EXCEPTION_POINTERS* ep) { LogException("P4 at load", ep); return EXCEPTION_EXECUTE_HANDLER; }
int SafeStartP4(uint64_t rnd)
{
	__try { StartP4DuringLoad(rnd); return 0; }
	__except (FilterLoad(GetExceptionInformation())) { return 1; }
}

// Loading screen, TK__Battle_SpawnPlayers2P: right after the game activates the Practice dummy (slot 2) of a 4-entry
// battle, prepare entry 3 and request P4's assets so they load with the others'
uint64_t HookActivateSlot(uint64_t rnd, uint64_t desc, uint64_t c, uint64_t d, uint64_t e8, uint64_t f8, uint64_t g8, uint64_t h8)
{
	HookGuard guard;
	const uint64_t ret = ((fn4)T("TAG_ActivateSlot"))(rnd, desc, c, d, e8, f8, g8, h8);
	if (kReloadMode && g_enabled && g_p4 >= 0 && desc && *(int32_t*)desc == 2 && SafeStartP4(rnd)) Log("P4 at load: exception caught");
	return ret;
}

// Sides >= 3 have no settings entry: the costume getters return defaults (costume type 0 / 0x33, static descriptor
// 0x1497834B0), fine for Paul, but another fighter loaded on side 3 came out as a bare mannequin (2026-11-14 22:47).
// For the sides this loader fills, answer with side 2's (the Practice dummy's) values, which dress any fighter (P3
// swapped into the dummy is dressed from them).
inline bool OurExtraSide(uint64_t side) { return g_enabled && (uint32_t)side >= 3 && (uint32_t)side < 8 && g_sideFighter[side] >= 0; }
uint64_t HookSideCostumeType(uint64_t settings, uint64_t side, uint64_t c, uint64_t d, uint64_t e8, uint64_t f8, uint64_t g8, uint64_t h8)
{
	HookGuard guard;
	return ((fn4)T("TAG_SideCostumeType"))(settings, OurExtraSide(side) ? 2 : side, c, d, e8, f8, g8, h8);
}
uint64_t HookSideDescriptor(uint64_t settings, uint64_t side, uint64_t c, uint64_t d, uint64_t e8, uint64_t f8, uint64_t g8, uint64_t h8)
{
	HookGuard guard;
	return ((fn4)T("TAG_SideDescriptor"))(settings, OurExtraSide(side) ? 2 : side, c, d, e8, f8, g8, h8);
}

uint64_t HookSideFighter(uint64_t settings, uint64_t side, uint64_t c, uint64_t d, uint64_t e8, uint64_t f8, uint64_t g8, uint64_t h8)
{
	HookGuard guard;
	// sides >= 3 have no settings descriptor: the fighter of the extra slot being loaded on that side
	if (g_enabled && (uint32_t)side >= 3 && (uint32_t)side < 8 && g_sideFighter[side] >= 0) return (uint64_t)(uint32_t)g_sideFighter[side];
	return ((fn4)T("TAG_SideFighter"))(settings, side, c, d, e8, f8, g8, h8);
}

uint64_t HookWallSensor(uint64_t p, uint64_t in, uint64_t out, uint64_t i, uint64_t e8, uint64_t f8, uint64_t g8, uint64_t h8)
{
	HookGuard guard;
	const uint64_t ret = ((fn4)T("TAG_WallSensor"))(p, in, out, i, e8, f8, g8, h8);
	if (IsPartner(p)) { memcpy((void*)out, (void*)in, 16); return ret & ~0xFFull; }   // walls intangible for partners
	return ret;
}

uint64_t HookSideButtons(uint64_t sidePtr, uint64_t b, uint64_t c, uint64_t d, uint64_t e8, uint64_t f8, uint64_t g8, uint64_t h8)
{
	HookGuard guard;
	uint64_t ret = ((fn4)T("TAG_SideButtons"))(sidePtr, b, c, d, e8, f8, g8, h8);
	if (!g_running || !sidePtr) return ret;
	const int s = *(int*)sidePtr;
	if (s != 0 && s != 1) return ret;
	const uint32_t bt = (uint32_t)ret;
	if ((bt & 0xF000) == 0xF000) {
		if (!g_kiLatch[s]) g_kiRequest[s] = true;
		g_kiLatch[s] = true;
		ret = (ret & ~0xFFFFFFFFull) | (bt & ~0xF000u);
	}
	else if ((bt & 0xF000) == 0) g_kiLatch[s] = false;
	return ret;
}

// Test input (PlayInput): one mask per call for the player, then back to the pad; its move ids are traced
struct ScriptedInput { uint64_t p = 0; int slot = -1; uint32_t masks[64] = {}; int count = 0, i = 0; uint32_t prev = 0; };
ScriptedInput g_script;
std::atomic_int g_scriptRequest = 0;    // set by the loader thread, taken by the frame tick
int g_scriptSlotPending = -1, g_scriptCountPending = 0;
uint32_t g_scriptMasksPending[64];
int g_traceFrames = 0; uint64_t g_traceP = 0; uint32_t g_traceLast = 0xFFFFFFFF; std::string g_traceLine;

uint64_t HookInputSource(uint64_t a, uint64_t p, uint64_t btn, uint64_t prs, uint64_t e8, uint64_t f8, uint64_t g8, uint64_t h8)
{
	HookGuard guard;
	const uint64_t ret = ((fn4)T("TAG_InputSource"))(a, p, btn, prs, e8, f8, g8, h8);
	if (g_script.p && p == g_script.p && g_script.i < g_script.count) {
		const uint32_t m = g_script.masks[g_script.i++];
		*(uint32_t*)btn = m; *(uint32_t*)prs = (m ^ g_script.prev) & m; g_script.prev = m;
		if (g_script.i >= g_script.count) g_script.p = 0;
		return (ret & ~0xFFull) | 1;
	}
	if (g_running && IsNeutral(p)) { *(uint32_t*)btn = 0; *(uint32_t*)prs = 0; return (ret & ~0xFFull) | 1; }
	return ret;
}

uint64_t HookClearMoves(uint64_t p, uint64_t b, uint64_t c, uint64_t d, uint64_t e8, uint64_t f8, uint64_t g8, uint64_t h8)
{
	// Practice restart clears the move pointers of all four entries but re-inits only the points: restore the
	// partners' three move pointers (+0x3BB0 stays cleared, as for the points) and re-init them a bit later
	++g_inHook;
	bool partner = false;
	uint64_t keep[3] = {};
	const int offs[3] = { O.clearedA, O.movePtr, O.clearedC };
	// only inside the Practice reset (View+A): a match reload also clears them, and must keep them cleared
	// partners, and any other ACTIVE player beyond the two points (the reset re-inits only P1 and P2; an active
	// Practice dummy left with null moves crashed the next frame, 22:47)
	__try {
		partner = g_inPracticeReset && g_running && (IsPartner(p) || (Rd<int32_t>(p + O.slot) >= 2 && Rd<uint8_t>(p + 8) == 1));
		if (partner) for (int i = 0; i < 3; ++i) keep[i] = Rd<uint64_t>(p + offs[i]);
	}
	__except (EXCEPTION_EXECUTE_HANDLER) { partner = false; }
	const uint64_t ret = ((fn4)T("TAG_ClearMoves"))(p, b, c, d, e8, f8, g8, h8);
	if (partner) {
		for (int i = 0; i < 3; ++i) Wr<uint64_t>(p + offs[i], keep[i]);
		if (!g_muteRestart && g_frames - g_restartFrame > 120) {
			g_restartFrame = g_frames;
			for (int t = 0; t < 2; ++t) { Finish(t); g_tq[t] = Queued{}; g_comboTeam[t] = false; g_pendingTag[t] = false; }
			g_snap = SNAP;
			g_reinitAt = g_frames + 18;
			for (int t = 0; t < 2; ++t) if (g_partnerSlot[t] >= 0) SetHidden(ActorOf(Entry(g_partnerSlot[t])), true);
			Log("Practice restart detected");
		}
	}
	--g_inHook;
	return ret;
}

uint64_t HookRootDeltaPass(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e8, uint64_t f8, uint64_t g8, uint64_t h8)
{
	HookGuard guard;
	const uint64_t ret = ((fn4)T("TAG_RootDeltaPass"))(a, b, c, d, e8, f8, g8, h8);
	if (g_running) for (auto& s : g_tr) {
		if (!s.active) continue;
		if (s.drive) WritePos(s.y, s.driveX, s.driveZ);
		if (s.holdN > 0) WritePos(s.y, s.holdX, s.holdZ);
	}
	return ret;
}

uint64_t HookPracticeReset(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e8, uint64_t f8, uint64_t g8, uint64_t h8)
{
	// the reset rebuilds per-fighter state in the layout the game built: put swapped teams back first
	++g_inHook;
	if (g_running) {
		__try {
			for (int t = 0; t < 2; ++t) {
				g_tq[t] = Queued{};
				Finish(t);
				if (g_revert[t].valid) {
					DoSwap(g_revert[t]);
					SetHidden(g_revert[t].ay, false); SetHidden(g_revert[t].ax, true);
					g_revert[t] = SwapInfo{}; g_swapped[t] = false;
					Log("Practice restart: team %c back in its original layout", 'A' + t);
				}
			}
		}
		__except (EXCEPTION_EXECUTE_HANDLER) { Log("Practice restart: swap-back failed"); }
	}
	g_inPracticeReset = true;
	const uint64_t ret = ((fn4)T("TAG_PracticeReset"))(a, b, c, d, e8, f8, g8, h8);
	g_inPracticeReset = false;
	--g_inHook;
	return ret;
}

using fn_xinput = DWORD(WINAPI*)(DWORD, void*);
DWORD WINAPI HookXInputGetState(DWORD slot, void* state)
{
	HookGuard guard;
	const DWORD ret = ((fn_xinput)T("TAG_XInputGetState"))(slot, state);
	if (ret != 0 || slot > 3 || !state || !g_running) return ret;
	const uint16_t b = *(uint16_t*)((uint8_t*)state + 4);
	const int side = slot == 0 ? 0 : 1;            // pad 0 = side 0 = team A, others team B
	if ((b & 0xF000) == 0xF000) { if (!g_kiLatch[side]) g_kiRequest[side] = true; g_kiLatch[side] = true; }
	else if ((b & 0xF000) == 0) g_kiLatch[side] = false;
	return ret;
}

// ---------------------------------------------------------------------------------------------------------------
// Hook installation with the other threads suspended (a thread must never run half-written jump bytes)
// ---------------------------------------------------------------------------------------------------------------
struct PendingHook { const char* name; uint64_t target; uint64_t hook; };

bool InstallHooksSuspended(const std::vector<PendingHook>& hooks)
{
	for (const auto& h : hooks) g_loader->InitHook(h.name, h.target, h.hook);
	// The hooking library allocates (CRT = process heap). A thread suspended while it holds the heap lock would
	// deadlock us with every other thread suspended (game frozen, 2026-11-14 22:01): take the heap lock FIRST, so no
	// suspended thread can hold it (it is recursive for this thread).
	const HANDLE heap = GetProcessHeap();
	for (int attempt = 0; attempt < 50; ++attempt) {
		HeapLock(heap);
		std::vector<HANDLE> suspended;
		suspended.reserve(1024);
		bool busy = false;
		HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
		THREADENTRY32 te{ sizeof(te) };
		const DWORD pid = GetCurrentProcessId(), me = GetCurrentThreadId();
		if (snap != INVALID_HANDLE_VALUE && Thread32First(snap, &te)) do {
			if (te.th32OwnerProcessID != pid || te.th32ThreadID == me) continue;
			HANDLE th = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT, FALSE, te.th32ThreadID);
			if (!th) continue;
			if (SuspendThread(th) == (DWORD)-1) { CloseHandle(th); continue; }
			suspended.push_back(th);
			CONTEXT ctx{}; ctx.ContextFlags = CONTEXT_CONTROL;
			if (GetThreadContext(th, &ctx))
				for (const auto& h : hooks) if (ctx.Rip >= h.target && ctx.Rip < h.target + 32) busy = true;
		} while (Thread32Next(snap, &te));
		if (snap != INVALID_HANDLE_VALUE) CloseHandle(snap);
		if (!busy) for (const auto& h : hooks) g_loader->HookFunction(h.name);
		for (auto th : suspended) { ResumeThread(th); CloseHandle(th); }
		HeapUnlock(heap);
		if (!busy) {
			for (const auto& h : hooks) if (!g_loader->HasHook(h.name)) return false;
			return true;
		}
		Sleep(2);
	}
	return false;
}

// ---------------------------------------------------------------------------------------------------------------
// Address resolution + byte checks
// ---------------------------------------------------------------------------------------------------------------
// Does the [f] pattern match the bytes at `at`? (the function was not moved by a game patch, and is not hooked by someone else)
bool PatternMatchesAt(const char* pattern, uint64_t at, std::string& why)
{
	std::vector<int> bytes;
	bool seenMarker = false;
	const char* c = pattern;
	while (*c) {
		while (*c == ' ' || *c == '\t') ++c;
		if (!*c) break;
		const char* e = c; while (*e && *e != ' ' && *e != '\t') ++e;
		std::string tok(c, e - c); c = e;
		if (tok == "[f]") { seenMarker = true; continue; }
		if (!seenMarker) continue;                     // bytes before the marker: not checked here
		if (tok == "??") bytes.push_back(-1);
		else if (tok.size() == 2) bytes.push_back((int)strtoul(tok.c_str(), nullptr, 16));
		else { why = "unsupported token " + tok; return false; }
	}
	if (!seenMarker) { why = "no [f] marker"; return false; }
	for (size_t i = 0; i < bytes.size(); ++i)
		if (bytes[i] >= 0 && Rd<uint8_t>(at + i) != (uint8_t)bytes[i]) { why = "bytes differ"; return false; }
	return true;
}

// Does the function start with a jump another mod wrote over its prologue? (UE4SS hooks UObject::ProcessEvent this
// way: jmp rel32, jmp [rip+0], or mov rax/r11, imm64 + jmp reg)
bool StartsWithHookJump(uint64_t at)
{
	const uint8_t b0 = Rd<uint8_t>(at), b1 = Rd<uint8_t>(at + 1);
	if (b0 == 0xE9) return true;                                                     // jmp rel32
	if (b0 == 0xFF && b1 == 0x25) return true;                                       // jmp [rip+disp32]
	if (b0 == 0x48 && b1 == 0xB8 && Rd<uint8_t>(at + 10) == 0xFF && Rd<uint8_t>(at + 11) == 0xE0) return true; // mov rax; jmp rax
	if (b0 == 0x49 && b1 == 0xBB && Rd<uint8_t>(at + 10) == 0x41 && Rd<uint8_t>(at + 11) == 0xFF
		&& Rd<uint8_t>(at + 12) == 0xE3) return true;                                // mov r11; jmp r11
	return false;
}

// Functions tag mode only calls (never hooks or patches). Another mod's hook on them forwards to the game's code, so
// calling through it is fine.
bool IsCallOnly(const char* key) { return !strcmp(key, "tag_process_event"); }
} // namespace

namespace TagT8
{
bool Init(MovesetLoader* loader, uint64_t moduleBase, uint64_t moduleSize, const GameAddressesWrapper& addresses, std::string& error)
{
	g_loader = loader; g_base = moduleBase; g_size = moduleSize;
	std::vector<std::string> missing;
	auto val = [&](const char* key) -> int64_t {
		int64_t v = 0;
		if (!addresses.addrFile->GetValueEx(addresses.gameKey, key, v)) { missing.push_back(key); return 0; }
		return v;
	};
	auto func = [&](const char* key) -> uint64_t {
		const int64_t rva = val(key);
		if (!rva) return 0;
		if ((uint64_t)rva >= moduleSize) { error += std::string(key) + " is outside the module; "; return 0; }
		const uint64_t at = moduleBase + (uint64_t)rva;
		const char* sig = nullptr;
		const std::string sigKey = std::string("sig_") + key + "_1";
		std::string why;
		if (addresses.addrFile->GetStringEx(addresses.gameKey, sigKey.c_str(), sig) && sig && !PatternMatchesAt(sig, at, why)) {
			if (StartsWithHookJump(at)) {
				if (IsCallOnly(key)) { Log("init: %s is hooked by another mod (UE4SS?); calling through its hook", key); return at; }
				why = "hooked by another mod (UE4SS?); remove it to use tag mode";
			}
			error += std::string(key) + ": " + why + "; ";
		}
		return at;
	};
	auto glob = [&](const char* key) -> uint64_t { const int64_t rva = val(key); return rva ? moduleBase + (uint64_t)rva : 0; };
	auto off = [&](const char* key) -> int { return (int)val(key); };

	F = Funcs{
		func("tag_frame"), func("tag_scene_ctor"), func("tag_set_gamemode"), func("tag_side_fighter"),
		func("tag_activate_slot"), func("tag_activate_slot_ue"), func("tag_get_player"), func("tag_get_player2"),
		func("tag_bind_fighter"), func("tag_clear_moves"), func("tag_alloc_helpers"), func("tag_set_cpu_status"),
		func("tag_set_active"), func("tag_init_player"), func("tag_init_cpu"), func("tag_get_move"),
		func("tag_input_source"), func("tag_side_buttons"), func("tag_wall_sensor"), func("tag_ps_get"),
		func("tag_ps_init"), func("tag_mothead_getcopy"), func("tag_moveset_relocate"), func("tag_init_moveset_parent"),
		func("tag_loader_world"), func("tag_get_loader"), func("tag_motion_request"), func("tag_motion_ready"),
		func("tag_root_delta_pass"), func("tag_practice_reset"), func("tag_process_event"),
		func("tag_malloc"), func("tag_battle_core_ctor"), func("tag_set_battle_registry"), func("tag_setup_battle"),
		func("tag_loadmatch_setup_ret"), func("tag_load_match"), func("tag_player_round_init"),
		func("tag_side_costume_type"), func("tag_side_descriptor"), func("tag_battle_core_dtor"),
		func("tag_animmap_contains"), func("tag_binder_common_ready"), func("tag_anim_popmap"),
		func("tag_mothead_store_release") };
	G = Globals{ glob("tag_registry"), glob("tag_round_key"), glob("tag_ps_key"), glob("tag_gamemode"),
		glob("tag_mothead_binder"), glob("tag_guobjectarray"), glob("tag_namepool"),
		glob("tag_anim_ref_bind"), glob("tag_umap_rehash"), glob("tag_vt_anim_ref"), glob("tag_vt_anim_factory") };
	O = Offsets{
		off("tag_p_slot"), off("tag_p_human"), off("tag_p_slot2"), off("tag_p_fighter"), off("tag_p_pos"),
		off("tag_p_facing"), off("tag_p_root"), off("tag_p_ready"), off("tag_p_mats"), off("tag_p_mat_t0"),
		off("tag_p_mat_t3"), off("tag_p_yaw"), off("tag_p_facing2"), off("tag_p_lookat"), off("tag_p_move_timer"),
		off("tag_p_move_ptr"), off("tag_p_vuln"), off("tag_p_anim_speed"), off("tag_p_move_id"),
		off("tag_p_in_reaction"), off("tag_p_next_move_ptr"), off("tag_p_next_move_id"), off("tag_p_input_mode"),
		off("tag_p_root_delta"), off("tag_p_prev_root"), off("tag_p_side"), off("tag_p_hp_slot"),
		off("tag_p_moveset"), off("tag_p_target"), off("tag_p_targeted"), off("tag_p_controller"),
		off("tag_p_cleared_a"), off("tag_p_cleared_c"),
		off("tag_m_table"), off("tag_m_count"), off("tag_m_size"), off("tag_m_cancel"), off("tag_m_transition"),
		off("tag_m_length"), off("tag_m_active_first"), off("tag_m_active_last"),
		off("tag_a_fighter"), off("tag_a_player_number"), off("tag_a_render_slot"), off("tag_a_master_mesh"),
		off("tag_cm_map"), off("tag_dm_map"), off("tag_s_dummy_fighter"), off("tag_s_desc"), off("tag_s_desc_stride"),
		off("tag_s_desc_fighter"), off("tag_e_to_player"), off("tag_r_slots"), off("tag_r_iter"), off("tag_r_storage"),
		off("tag_r_slots2"), off("tag_r_iter2"), off("tag_reg_sentinel"), off("tag_reg_buckets"), off("tag_reg_mask"),
		off("tag_gm_sentinel"), off("tag_gm_count"), off("tag_gm_buckets"), off("tag_gm_mask"), off("tag_gm_key") };
	for (const auto& m : missing) error += m + " missing; ";
	if (!error.empty()) {
		error = "game_addresses.ini (t8-retail_tag_*) does not fit this game: " + error + "run Tools > Addresses.";
		Log("init failed: %s", error.c_str());
		return false;
	}
	g_initialized = true;
	Log("init: addresses resolved and checked (module %llx)", moduleBase);
	{
		std::string growError;
		g_growReady = TagT8Grow::ResolveAddresses(moduleBase, moduleSize, addresses, g_grow, growError);
		if (!g_growReady) Log("in-place grow unavailable: %s", growError.c_str());
	}
	return true;
}

bool IsInitialized() { return g_initialized; }

bool Enable(int p3, int p4, std::string& error)
{
	if (!g_initialized) { error = "not initialized"; return false; }
	if (p3 < 0) { error = "P3 not set"; return false; }
	if (!g_hooked) {
		if (!G.guobjectarray || !ResolveReflection()) { error = "Unreal reflection lookups failed"; return false; }
		std::vector<PendingHook> hooks = {
			{ "TAG_Frame", F.frame, (uint64_t)&HookFrame },
			{ "TAG_SceneCtor", F.sceneCtor, (uint64_t)&HookSceneCtor },
			{ "TAG_SetGamemode", F.setGamemode, (uint64_t)&HookSetGamemode },
			{ "TAG_SideFighter", F.sideFighter, (uint64_t)&HookSideFighter },
			{ "TAG_WallSensor", F.wallSensor, (uint64_t)&HookWallSensor },
			{ "TAG_SideButtons", F.sideButtons, (uint64_t)&HookSideButtons },
			{ "TAG_InputSource", F.inputSource, (uint64_t)&HookInputSource },
			{ "TAG_ClearMoves", F.clearMoves, (uint64_t)&HookClearMoves },
			{ "TAG_RootDeltaPass", F.rootDeltaPass, (uint64_t)&HookRootDeltaPass },
			{ "TAG_PracticeReset", F.practiceReset, (uint64_t)&HookPracticeReset },
			{ "TAG_ActivateSlot", F.activateSlot, (uint64_t)&HookActivateSlot },
			{ "TAG_SetupBattle", F.setupBattle, (uint64_t)&HookSetupBattle },
			{ "TAG_LoadMatch", F.loadMatch, (uint64_t)&HookLoadMatch },
			{ "TAG_PlayerRoundInit", F.playerRoundInit, (uint64_t)&HookPlayerRoundInit },
			{ "TAG_SideCostumeType", F.sideCostumeType, (uint64_t)&HookSideCostumeType },
			{ "TAG_SideDescriptor", F.sideDescriptor, (uint64_t)&HookSideDescriptor },
		};
		for (const char* dll : { "XINPUT1_4.dll", "XINPUT1_3.dll", "XINPUT9_1_0.dll" }) {
			if (HMODULE m = GetModuleHandleA(dll)) {
				if (auto x = GetProcAddress(m, "XInputGetState")) {
					// The export is a forwarding stub (jmp [rip+d] / jmp rel32) in XINPUT1_4: hook its target. A
					// target that starts with a jump too is someone else's hook: leave it, Ki Charge still comes
					// through the game's side-button read.
					uint64_t at = (uint64_t)x;
					for (int k = 0; k < 2; ++k) {
						const uint8_t* b = (const uint8_t*)at;
						if (b[0] == 0xFF && b[1] == 0x25) at = *(uint64_t*)(at + 6 + *(int32_t*)(at + 2));
						else if (b[0] == 0xE9) at = at + 5 + *(int32_t*)(at + 1);
						else break;
					}
					const uint8_t first = *(uint8_t*)at;
					if (first == 0xE9 || first == 0xEB || (first == 0xFF && ((const uint8_t*)at)[1] == 0x25)) { Log("XInput: %s target %llx is already hooked, X+Y+A+B not read", dll, at); break; }
					hooks.push_back({ "TAG_XInputGetState", at, (uint64_t)&HookXInputGetState }); Log("XInput: %s (target %llx)", dll, at); break;
				}
			}
		}
		if (!InstallHooksSuspended(hooks)) { error = "could not install the hooks"; Log("hook install failed"); return false; }
		g_hooked = true;
		Log("hooks installed (%zu)", hooks.size());
	}
	g_p3 = p3; g_p4 = p4;
	g_enabled = true;
	if (!g_running && g_setup == Setup::Idle)
		SetStatus(TagState_WaitingForPractice, "Tag on (P3 %d, P4 %d): enter Practice (or re-enter it from the menu) to load them", p3, p4);
	return true;
}

bool Disable()
{
	// refused while P3/P4 are being added: the tool greys the button out, this also covers headless / racing requests
	if (SetupInProgress()) { Log("disable refused: P3/P4 are still loading"); return false; }
	g_enabled = false;
	if (!g_running && g_setup == Setup::Idle) SetStatus(TagState_Off, "Tag: off");
	else SetStatus(g_state, "Tag: off for the next Practice (this match keeps Tag until it ends)");
}

void Shutdown()
{
	if (!g_hooked) return;
	g_enabled = false;
	g_shuttingDown = true;
	static const char* names[] = { "TAG_Frame", "TAG_SceneCtor", "TAG_SetGamemode", "TAG_SideFighter", "TAG_WallSensor",
		"TAG_SideButtons", "TAG_InputSource", "TAG_ClearMoves", "TAG_RootDeltaPass", "TAG_PracticeReset", "TAG_XInputGetState",
		"TAG_ActivateSlot", "TAG_SetupBattle", "TAG_LoadMatch", "TAG_PlayerRoundInit", "TAG_SideCostumeType", "TAG_SideDescriptor" };
	HMODULE self = nullptr;
	GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)&Shutdown, &self);
	const uint64_t lo = (uint64_t)self;
	uint64_t hi = lo;
	if (self) { auto dos = (IMAGE_DOS_HEADER*)self; hi = lo + ((IMAGE_NT_HEADERS*)(lo + dos->e_lfanew))->OptionalHeader.SizeOfImage; }
	// Unhook only at an instant where no thread runs a hook body (counted) or our code, with every thread suspended
	const HANDLE heap = GetProcessHeap();
	for (int attempt = 0; attempt < 400; ++attempt) {
		HeapLock(heap);                   // see InstallHooksSuspended: no suspended thread may hold the heap lock
		std::vector<HANDLE> suspended;
		suspended.reserve(1024);
		bool busy = false;
		HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
		THREADENTRY32 te{ sizeof(te) };
		const DWORD pid = GetCurrentProcessId(), me = GetCurrentThreadId();
		if (snap != INVALID_HANDLE_VALUE && Thread32First(snap, &te)) do {
			if (te.th32OwnerProcessID != pid || te.th32ThreadID == me) continue;
			HANDLE th = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT, FALSE, te.th32ThreadID);
			if (!th) continue;
			if (SuspendThread(th) == (DWORD)-1) { CloseHandle(th); continue; }
			suspended.push_back(th);
			CONTEXT ctx{}; ctx.ContextFlags = CONTEXT_CONTROL;
			if (GetThreadContext(th, &ctx) && ctx.Rip >= lo && ctx.Rip < hi) busy = true;
		} while (Thread32Next(snap, &te));
		if (snap != INVALID_HANDLE_VALUE) CloseHandle(snap);
		if (g_inHook.load() != 0) busy = true;
		if (!busy) for (auto n : names) if (g_loader->HasHook(n)) g_loader->UnhookFunction(n);
		for (auto th : suspended) { ResumeThread(th); CloseHandle(th); }
		HeapUnlock(heap);
		if (!busy) { g_hooked = false; Log("hooks removed (attempt %d)", attempt + 1); return; }
		Sleep(5);
	}
	Log("hooks could not be removed safely");
}

void RequestTagFor(int team) { if (team == 0 || team == 1) { g_kiRequest[team] = true; Log("tag %c requested by the tool", 'A' + team); } }

// Loading for as long as P3/P4 are being added (grow pending or a slot setup running), whatever the last status line
// said: the status written right after the grow came before the pipeline started, so it read Running for the whole
// load and the tool left Disable enabled (2026-11-15)
int State() { const int s = g_state; return s == TagState_Running && SetupInProgress() ? TagState_Loading : s; }
bool Loading() { return SetupInProgress(); }
std::string Status() { std::lock_guard lock(g_statusMutex); return g_status; }
}
