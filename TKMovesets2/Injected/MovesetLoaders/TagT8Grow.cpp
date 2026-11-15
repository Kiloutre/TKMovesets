// In-place 3 -> 4 slot grow of a running retail Tekken 8 battle (see TagT8Grow.hpp).
// Layout evidence (retail 26155785, RE repo work/retail/fourslot/): battle core ctor 0x1418E4350 {vt, +8 count,
// +0x10 = registry object 0x1418E3890 (0x138 bytes, services at +8*slot)}; the 11 count-sized service ctors are in
// svc_*.c / grow/reg/*.c there. Write-up: RE repo docs/tag-retail.md "Growing a running battle to 4 slots".
//
// Every structure sized from the count is an MSVC std::vector {begin, end, cap} (length = end - begin). The routine
// never runs a destructor and never frees: old buffers and throwaway objects are leaked, so a pointer that something
// kept into them stays readable (stale, not dangling).

#include "TagT8Grow.hpp"

#include <windows.h>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <new>

#ifndef TAGT8GROW_NO_RESOLVER
#include "GameAddressesWrapper.hpp"
#endif

namespace TagT8Grow
{
const char* const kCtorKeys[kServiceCount] = {
	"tag_grow_ctor_s04", "tag_grow_ctor_s06", "tag_grow_ctor_s09", "tag_grow_ctor_s0a", "tag_grow_ctor_s14",
	"tag_grow_ctor_s16", "tag_grow_ctor_s17", "tag_grow_ctor_s18", "tag_grow_ctor_s19", "tag_grow_ctor_s1b",
	"tag_grow_ctor_s1c",
};
}

namespace
{
using namespace TagT8Grow;
using u64 = uint64_t;

template<class T> inline T Rd(u64 a) { return *(volatile T*)a; }
template<class T> inline void Wr(u64 a, T v) { *(volatile T*)a = v; }

constexpr u64 kCoreCount = 0x8, kCoreRegistry = 0x10;
constexpr u64 kRoundEntrySize = 0x1F140;               // TK__Malloc(0x1f140) in the round ctor 0x141918520
constexpr u64 kRoundTables[] = { 0x8, 0x50 };          // round +0x08 / +0x50: 0x200-byte slot tables (not count-sized)
constexpr u64 kRoundEntryVecs[] = { 0x38, 0x68 };      // the two count-sized entry-pointer vectors

struct SvcDef { int slot; uint32_t size; bool vtable; };
// registry slot, object size (TK__Malloc in 0x1418E3890 / 0x141918B80), first qword is a vtable
const SvcDef kSvc[kServiceCount] = {
	{ 0x04, 0x1AA0, true },     // 0x1418722D0
	{ 0x06, 0x88,   true },     // round 0x141918520 (slot 6 = 8-byte holder -> round)
	{ 0x09, 0x160,  false },    // 0x1419022C0
	{ 0x0A, 0x18,   false },    // 0x14194E7B0
	{ 0x14, 0x58,   true },     // 0x14195EE00
	{ 0x16, 0x1E0,  true },     // 0x1417F5CB0
	{ 0x17, 0x30,   true },     // 0x141831A10
	{ 0x18, 0x78,   true },     // 0x14194DE90
	{ 0x19, 0xC38,  true },     // 0x14194E220
	{ 0x1B, 0x40,   true },     // 0x1418E4180
	{ 0x1C, 0x1A0,  true },     // 0x141974B30
};

enum Kind : uint8_t {
	kPlain,      // n elements of `elem` bytes
	kNodeBack,   // 0x1418E4180 +0x10: element[0] = heap node whose qword 0 points back at the element
	kBits,       // std::vector<bool>: vector<uint32_t> words at off, bit count at off+0x18
};

constexpr int kMaxVecs = 192;   // 164 used
constexpr int kMaxRegions = 256;

struct VecPlan {
	int svc; uint32_t off, elem; Kind kind;
	u64 b, e, c;          // live vector before
	u64 tb, te, tc;       // throwaway's vector
	u64 nb;               // new buffer (0: grown in place)
};
struct Reloc { u64 lo, hi, to; int svc; uint32_t off; };
struct Region { u64 base, size; uint8_t kind, slot; uint16_t off; uint8_t entry; };

struct Plan {
	const GrowAddresses* a;
	u64 core, reg;
	int n, m, target;
	u64 obj[kServiceCount], tmp[kServiceCount];
	VecPlan v[kMaxVecs]; int nv;
	Reloc r[kMaxVecs]; int nr;
	Region g[kMaxRegions]; int ng;
	GrowReport rep;
	const char* stage;
	bool committing;
	char err[300];
};

bool Fail(Plan& p, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vsnprintf(p.err, sizeof(p.err), fmt, args);
	va_end(args);
	return false;
}

void AddVec(Plan& p, int svc, uint32_t off, uint32_t elem, Kind kind = kPlain)
{
	VecPlan& v = p.v[p.nv++];
	memset(&v, 0, sizeof(v));
	v.svc = svc; v.off = off; v.elem = elem; v.kind = kind;
}

void BuildVecList(Plan& p)
{
	p.nv = 0;
	AddVec(p, kS04, 0x378, 8);
	for (u64 o : kRoundEntryVecs) AddVec(p, kRound, (uint32_t)o, 8);
	AddVec(p, kS09, 0x0, 0x148);
	AddVec(p, kS0A, 0x0, 0x50);
	AddVec(p, kS14, 0x08, 0x2A0);
	AddVec(p, kS14, 0x20, 0x40);      // unordered_map elements, capacity reserved to 0x400 by the ctor
	AddVec(p, kS14, 0x38, 8);
	AddVec(p, kS16, 0x08, 0x7B8);     // TK__Round_BuildEntries 0x1417F5790(vec, count): per-slot records, indexed by
	                                  // player+0x160 (0x141806A40 writes rec[slot]+0x720); owns a vector at +0x7A0
	AddVec(p, kS16, 0x20, 0x12C0);
	AddVec(p, kS17, 0x10, 8);
	AddVec(p, kS18, 0x08, 4);
	AddVec(p, kS18, 0x20, 0, kBits);
	AddVec(p, kS18, 0x40, 0, kBits);
	for (uint32_t i = 0; i < 65; ++i) {   // 65 history frames
		AddVec(p, kS19, 0x08 + 0x30 * i, 8);
		AddVec(p, kS19, 0x20 + 0x30 * i, 4);
	}
	AddVec(p, kS1B, 0x10, 0x28, kNodeBack);
	AddVec(p, kS1B, 0x28, 4);
	for (uint32_t i = 0; i < 8; ++i) AddVec(p, kS1C, 0x08 + 0x18 * i, 0xC0);
	AddVec(p, kS1C, 0xC8, 0x80);
	for (uint32_t i = 0; i < 8; ++i) AddVec(p, kS1C, 0xE0 + 0x18 * i, 0x80);
}

using fn_malloc = u64(*)(u64);
using fn_ctor = u64(*)(u64, int*);

// MSVC std::allocator rule (the ctors' own code): >= 0x1000 bytes -> +0x27, 32-byte aligned, raw pointer at [-8]
u64 AllocVecBuffer(const GrowAddresses& a, u64 bytes)
{
	const fn_malloc m = (fn_malloc)a.malloc;
	if (bytes < 0x1000) return m(bytes);
	const u64 raw = m(bytes + 0x27);
	if (!raw) return 0;
	const u64 al = (raw + 0x27) & ~(u64)0x1F;
	Wr<u64>(al - 8, raw);
	return al;
}

int BitWords(int bits) { return (bits + 31) / 32; }

bool ReadVec(u64 obj, const VecPlan& v, u64& b, u64& e, u64& c)
{
	b = Rd<u64>(obj + v.off); e = Rd<u64>(obj + v.off + 8); c = Rd<u64>(obj + v.off + 0x10);
	return true;
}

// ---- phase 1: the live battle has the expected layout --------------------------------------------------------
bool Validate(Plan& p)
{
	p.stage = "validate";
	p.reg = Rd<u64>(p.core + kCoreRegistry);
	if (!p.reg) return Fail(p, "battle core %llx has no registry object", p.core);
	for (int s = 0; s < kServiceCount; ++s) {
		u64 o = Rd<u64>(p.reg + 8ull * kSvc[s].slot);
		if (o && s == kRound) o = Rd<u64>(o);
		if (!o) return Fail(p, "registry slot 0x%X is empty", kSvc[s].slot);
		p.obj[s] = o;
	}
	for (int i = 0; i < p.nv; ++i) {
		VecPlan& v = p.v[i];
		ReadVec(p.obj[v.svc], v, v.b, v.e, v.c);
		if (!v.b || v.e < v.b || v.c < v.e)
			return Fail(p, "slot 0x%X +0x%X: not a vector (%llx %llx %llx)", kSvc[v.svc].slot, v.off, v.b, v.e, v.c);
		if (v.kind == kBits) {
			const u64 bits = Rd<u64>(p.obj[v.svc] + v.off + 0x18);
			if (bits != (u64)p.n || v.e - v.b != 4ull * BitWords(p.n) || v.c - v.b < 4ull * BitWords(p.target))
				return Fail(p, "slot 0x%X +0x%X: bit vector of %llu bits / %llu bytes, expected %d", kSvc[v.svc].slot,
					v.off, bits, v.e - v.b, p.n);
			continue;
		}
		if (v.e - v.b != (u64)p.n * v.elem)
			return Fail(p, "slot 0x%X +0x%X: %llu bytes, expected %d x 0x%X (count cached elsewhere, or not count-sized)",
				kSvc[v.svc].slot, v.off, v.e - v.b, p.n, v.elem);
		if (v.kind == kNodeBack)
			for (int k = 0; k < p.n; ++k) {
				const u64 el = v.b + (u64)k * v.elem, node = Rd<u64>(el);
				if (!node || Rd<u64>(node) != el) return Fail(p, "slot 0x%X element %d: node does not point back", kSvc[v.svc].slot, k);
			}
	}
	// the round's old entries (scanned for pointers later) exist
	for (u64 o : kRoundEntryVecs)
		for (int k = 0; k < p.n; ++k)
			if (!Rd<u64>(Rd<u64>(p.obj[kRound] + o) + 8ull * k)) return Fail(p, "round +0x%llX entry %d is null", o, k);
	return true;
}

// ---- phase 2: throwaway services with count 4-n, built by the game's ctors ------------------------------------
bool BuildTemps(Plan& p)
{
	p.stage = "build";
	const fn_malloc m = (fn_malloc)p.a->malloc;
	for (int s = 0; s < kServiceCount; ++s) {
		const u64 t = m(kSvc[s].size);
		if (!t) return Fail(p, "out of memory (service slot 0x%X)", kSvc[s].slot);
		memset((void*)t, 0, kSvc[s].size);
		int count = p.m;
		((fn_ctor)p.a->ctor[s])(t, &count);
		p.tmp[s] = t;
		p.rep.bytesLeaked += kSvc[s].size;
		if (kSvc[s].vtable && Rd<u64>(t) != Rd<u64>(p.obj[s]))
			return Fail(p, "slot 0x%X: the ctor's object (vt %llx) is not the live one (vt %llx)", kSvc[s].slot, Rd<u64>(t), Rd<u64>(p.obj[s]));
	}
	for (int i = 0; i < p.nv; ++i) {
		VecPlan& v = p.v[i];
		if (v.kind == kBits) continue;                 // fresh bits are false: nothing taken from the throwaway
		ReadVec(p.tmp[v.svc], v, v.tb, v.te, v.tc);
		if (!v.tb || v.te - v.tb != (u64)p.m * v.elem)
			return Fail(p, "slot 0x%X +0x%X: the ctor built %llu bytes for count %d (expected 0x%X each)",
				kSvc[v.svc].slot, v.off, v.te - v.tb, p.m, v.elem);
		if (v.kind == kNodeBack)
			for (int k = 0; k < p.m; ++k) {
				const u64 el = v.tb + (u64)k * v.elem, node = Rd<u64>(el);
				if (!node || Rd<u64>(node) != el) return Fail(p, "slot 0x%X fresh element %d: node does not point back", kSvc[v.svc].slot, k);
			}
	}
	return true;
}

// ---- phase 3: every new buffer, before anything live changes -------------------------------------------------
bool Allocate(Plan& p)
{
	p.stage = "allocate";
	for (int i = 0; i < p.nv; ++i) {
		VecPlan& v = p.v[i];
		if (v.kind == kBits) continue;
		const u64 want = (u64)p.target * v.elem;
		if (v.c - v.b >= want) { v.nb = 0; continue; }  // fits in the capacity: grown in place
		v.nb = AllocVecBuffer(*p.a, want);
		if (!v.nb) return Fail(p, "out of memory (slot 0x%X +0x%X, 0x%llX bytes)", kSvc[v.svc].slot, v.off, want);
	}
	return true;
}

// ---- phase 4: commit (no failure path) -----------------------------------------------------------------------
void AddRegion(Plan& p, u64 base, u64 size, uint8_t kind, int slot, u64 off = 0, int entry = 0)
{
	if (base && size && p.ng < kMaxRegions) p.g[p.ng++] = { base, size, kind, (uint8_t)slot, (uint16_t)off, (uint8_t)entry };
}

void Hit(PointerHit* list, int& count, const Region& g, u64 q, const Reloc& r, u64 val)
{
	if (count >= kMaxHits) return;
	PointerHit& h = list[count++];
	h.region = g.kind; h.regionSlot = g.slot; h.regionOff = g.off; h.entry = g.entry;
	h.at = (uint32_t)(q - g.base); h.vecSlot = (uint8_t)kSvc[r.svc].slot; h.vecOff = (uint16_t)r.off;
	h.into = (uint32_t)(val - r.lo);
}

void Commit(Plan& p)
{
	p.stage = "commit";
	p.committing = true;
	p.nr = 0;
	for (int i = 0; i < p.nv; ++i) {
		VecPlan& v = p.v[i];
		const u64 obj = p.obj[v.svc] + v.off;
		if (v.kind == kBits) {
			Wr<u64>(obj + 0x18, (u64)p.target);
			const uint32_t fresh = ((1u << p.target) - 1) & ~((1u << p.n) - 1);
			Wr<uint32_t>(v.b, Rd<uint32_t>(v.b) & ~fresh);
			++p.rep.vectorsInPlace;
			continue;
		}
		const u64 oldBytes = (u64)p.n * v.elem, newBytes = (u64)p.m * v.elem;
		if (!v.nb) {
			memcpy((void*)v.e, (const void*)v.tb, newBytes);
			Wr<u64>(obj + 8, v.e + newBytes);
			++p.rep.vectorsInPlace;
		}
		else {
			memcpy((void*)v.nb, (const void*)v.b, oldBytes);
			memcpy((void*)(v.nb + oldBytes), (const void*)v.tb, newBytes);
			Wr<u64>(obj, v.nb);
			Wr<u64>(obj + 8, v.nb + oldBytes + newBytes);
			Wr<u64>(obj + 0x10, v.nb + oldBytes + newBytes);
			p.r[p.nr++] = { v.b, v.b + oldBytes, v.nb, v.svc, v.off };
			p.rep.bytesLeaked += v.c - v.b;
			++p.rep.vectorsReallocated;
		}
		// the fresh elements now belong to the live vector: the throwaway forgets them (it is never destroyed anyway)
		const u64 t = p.tmp[v.svc] + v.off;
		Wr<u64>(t, 0); Wr<u64>(t + 8, 0); Wr<u64>(t + 0x10, 0);
		if (v.kind == kNodeBack) {
			const u64 b = Rd<u64>(obj);
			for (int k = 0; k < p.target; ++k) {
				const u64 el = b + (u64)k * v.elem;
				Wr<u64>(Rd<u64>(el), el);
			}
		}
	}

	// pointers into the old buffers, in the battle memory we know: the live services, every vector's buffer, the
	// round's tables and its old entries
	p.ng = 0;
	for (int s = 0; s < kServiceCount; ++s) AddRegion(p, p.obj[s], kSvc[s].size, 0, kSvc[s].slot);
	for (int i = 0; i < p.nv; ++i) {
		const VecPlan& v = p.v[i];
		if (v.kind == kBits) continue;
		const u64 b = Rd<u64>(p.obj[v.svc] + v.off);
		AddRegion(p, b, (u64)p.target * v.elem, 1, kSvc[v.svc].slot, v.off);
	}
	const u64 rnd = p.obj[kRound];
	for (u64 o : kRoundTables) AddRegion(p, Rd<u64>(rnd + o), Rd<u64>(rnd + o + 8) - Rd<u64>(rnd + o), 2, 6, o);
	for (u64 o : kRoundEntryVecs)
		for (int k = 0; k < p.n; ++k) AddRegion(p, Rd<u64>(Rd<u64>(rnd + o) + 8ull * k), kRoundEntrySize, 3, 6, o, k);

	// sort relocations by lo (<= ~170, insertion sort)
	for (int i = 1; i < p.nr; ++i) {
		const Reloc x = p.r[i];
		int j = i - 1;
		while (j >= 0 && p.r[j].lo > x.lo) { p.r[j + 1] = p.r[j]; --j; }
		p.r[j + 1] = x;
	}
	if (p.nr) {
		const u64 lo = p.r[0].lo;
		u64 hi = 0;
		for (int i = 0; i < p.nr; ++i) if (p.r[i].hi > hi) hi = p.r[i].hi;
		for (int gi = 0; gi < p.ng; ++gi) {
			const u64 base = (p.g[gi].base + 7) & ~(u64)7, end = p.g[gi].base + p.g[gi].size;
			for (u64 q = base; q + 8 <= end; q += 8) {
				const u64 val = Rd<u64>(q);
				if (val < lo || val > hi) continue;
				int a = 0, b = p.nr - 1, found = -1;    // last reloc with lo <= val
				while (a <= b) { const int mid = (a + b) / 2; if (p.r[mid].lo <= val) { found = mid; a = mid + 1; } else b = mid - 1; }
				if (found < 0) continue;
				if (val < p.r[found].hi) {
					Hit(p.rep.relocated, p.rep.relocatedHits, p.g[gi], q, p.r[found], val);
					Wr<u64>(q, val - p.r[found].lo + p.r[found].to);
					++p.rep.pointersRelocated;
				}
				else if (val == p.r[found].hi) {
					Hit(p.rep.stale, p.rep.staleHits, p.g[gi], q, p.r[found], val);
					++p.rep.staleEndPointers;
				}
			}
		}
	}

	Wr<int32_t>(p.core + kCoreCount, p.target);
	p.rep.newCount = p.target;
	p.committing = false;
}

bool Run(Plan& p)
{
	BuildVecList(p);
	if (!Validate(p)) return false;
	if (!BuildTemps(p)) return false;
	if (!Allocate(p)) return false;
	Commit(p);
	return true;
}

int Filter(Plan* p, EXCEPTION_POINTERS* ep)
{
	const EXCEPTION_RECORD* r = ep->ExceptionRecord;
	snprintf(p->err, sizeof(p->err), "%s during %s: exception %08lX at %p (address %llx)",
		p->committing ? "FAULT" : "fault", p->stage, r->ExceptionCode, r->ExceptionAddress,
		r->NumberParameters >= 2 ? (unsigned long long)r->ExceptionInformation[1] : 0ull);
	return EXCEPTION_EXECUTE_HANDLER;
}

// SEH-only helpers: no C++ objects with destructors in these
bool SafeReadI32(u64 a, int32_t& out) { __try { out = Rd<int32_t>(a); return true; } __except (EXCEPTION_EXECUTE_HANDLER) { return false; } }
bool SafeReadU64(u64 a, u64& out) { __try { out = Rd<u64>(a); return true; } __except (EXCEPTION_EXECUTE_HANDLER) { return false; } }

int SafeRun(Plan* p)
{
	__try { return Run(*p) ? 1 : 0; }
	__except (Filter(p, GetExceptionInformation())) { return -1; }
}
} // namespace

namespace TagT8Grow
{
bool GrowCoreTo(const GrowAddresses& a, uint64_t core, int target, std::string& error, GrowReport* report)
{
	if (report) *report = GrowReport{};
	if (target < 2 || target > kMaxTarget) { error = "TagT8Grow: target count " + std::to_string(target) + " (2.." + std::to_string(kMaxTarget) + ")"; return false; }
	if (!a.malloc) { error = "TagT8Grow: no malloc address"; return false; }
	for (int s = 0; s < kServiceCount; ++s)
		if (!a.ctor[s]) { error = std::string("TagT8Grow: no address for ") + kCtorKeys[s]; return false; }
	if (!core) { error = "TagT8Grow: no battle core"; return false; }

	int32_t n = 0;
	if (!SafeReadI32(core + kCoreCount, n)) { error = "TagT8Grow: battle core unreadable"; return false; }
	if (report) { report->oldCount = n; report->newCount = n; }
	if (n >= target) return true;
	if (n < 1) { error = "TagT8Grow: battle count " + std::to_string(n); return false; }

	Plan* p = new (std::nothrow) Plan;
	if (!p) { error = "TagT8Grow: out of memory"; return false; }
	memset(p, 0, sizeof(*p));
	p->a = &a; p->core = core; p->n = n; p->m = target - n; p->target = target;
	p->rep.oldCount = n; p->rep.newCount = n;
	p->stage = "start";
	const int r = SafeRun(p);
	if (report) *report = p->rep;
	if (r != 1) {
		error = std::string("TagT8Grow: ") + p->err;
		if (r < 0 && p->committing) error = std::string(p->err) + " (TagT8Grow: the battle may be inconsistent)";
	}
	delete p;
	return r == 1;
}

bool GrowBattleTo(const GrowAddresses& a, int target, std::string& error, GrowReport* report)
{
	if (!a.registry) { error = "TagT8Grow: no registry address"; return false; }
	u64 core = 0;
	if (!SafeReadU64(a.registry, core)) { error = "TagT8Grow: registry unreadable"; return false; }
	return GrowCoreTo(a, core, target, error, report);
}
bool GrowBattleTo4(const GrowAddresses& a, std::string& error, GrowReport* report) { return GrowBattleTo(a, 4, error, report); }
bool GrowCoreTo4(const GrowAddresses& a, uint64_t core, std::string& error, GrowReport* report) { return GrowCoreTo(a, core, 4, error, report); }

std::string Describe(const GrowReport& r)
{
	char buf[256];
	snprintf(buf, sizeof(buf), "battle %d -> %d slots: %d vectors reallocated, %d grown in place, %d pointers relocated, "
		"%d stale end pointers, %llu bytes leaked", r.oldCount, r.newCount,
		r.vectorsReallocated, r.vectorsInPlace, r.pointersRelocated, r.staleEndPointers, (unsigned long long)r.bytesLeaked);
	std::string out = buf;
	static const char* const kinds[] = { "service", "buffer of", "round table", "round entry" };
	auto list = [&](const char* name, const PointerHit* h, int n) {
		for (int i = 0; i < n; ++i) {
			char where[64];
			if (h[i].region == 0) snprintf(where, sizeof(where), "%s s%02X", kinds[0], h[i].regionSlot);
			else if (h[i].region == 3) snprintf(where, sizeof(where), "%s round+0x%X[%d]", kinds[3], h[i].regionOff, h[i].entry);
			else snprintf(where, sizeof(where), "%s s%02X+0x%X", kinds[h[i].region & 3], h[i].regionSlot, h[i].regionOff);
			snprintf(buf, sizeof(buf), "; %s: %s +0x%X -> s%02X+0x%X old buffer +0x%X", name, where, h[i].at,
				h[i].vecSlot, h[i].vecOff, h[i].into);
			out += buf;
		}
	};
	list("relocated", r.relocated, r.relocatedHits);
	list("stale end", r.stale, r.staleHits);
	return out;
}

#ifndef TAGT8GROW_NO_RESOLVER
namespace
{
// 0 match, 1 bytes differ, 2 bad pattern, 3 unreadable. Only the bytes after the [f] marker are checked.
int PatternAtRaw(const char* pattern, u64 at)
{
	__try {
		bool marker = false;
		u64 i = 0;
		for (const char* c = pattern; *c;) {
			while (*c == ' ' || *c == '\t') ++c;
			if (!*c) break;
			const char* e = c; while (*e && *e != ' ' && *e != '\t') ++e;
			const size_t len = (size_t)(e - c);
			if (len == 3 && !strncmp(c, "[f]", 3)) marker = true;
			else if (marker) {
				if (len != 2) return 2;
				if (c[0] != '?') {
					const char hex[3] = { c[0], c[1], 0 };
					if (Rd<uint8_t>(at + i) != (uint8_t)strtoul(hex, nullptr, 16)) return 1;
				}
				++i;
			}
			c = e;
		}
		return marker ? 0 : 2;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) { return 3; }
}
}

bool ResolveAddresses(uint64_t moduleBase, uint64_t moduleSize, const GameAddressesWrapper& addresses,
	GrowAddresses& out, std::string& error)
{
	out = GrowAddresses{};
	std::string err;
	auto rva = [&](const char* key) -> u64 {
		int64_t v = 0;
		if (!addresses.addrFile || !addresses.addrFile->GetValueEx(addresses.gameKey, key, v) || v <= 0) {
			err += std::string(key) + " missing; ";
			return 0;
		}
		if ((u64)v >= moduleSize) { err += std::string(key) + " outside the module; "; return 0; }
		return moduleBase + (u64)v;
	};
	auto func = [&](const char* key) -> u64 {
		const u64 at = rva(key);
		if (!at) return 0;
		const char* sig = nullptr;
		const std::string sigKey = std::string("sig_") + key + "_1";
		if (!addresses.addrFile->GetStringEx(addresses.gameKey, sigKey.c_str(), sig) || !sig) { err += sigKey + " missing; "; return 0; }
		static const char* const why[] = { "", "bytes differ", "bad pattern", "unreadable" };
		const int r = PatternAtRaw(sig, at);
		if (r) { err += std::string(key) + ": " + why[r] + "; "; return 0; }
		return at;
	};
	out.malloc = func("tag_malloc");
	out.registry = rva("tag_registry");
	for (int s = 0; s < kServiceCount; ++s) out.ctor[s] = func(kCtorKeys[s]);
	if (!err.empty()) { error = "TagT8Grow: " + err; out = GrowAddresses{}; return false; }
	return true;
}
#endif
} // namespace TagT8Grow
