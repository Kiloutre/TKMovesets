// Kamui hash (Tekken 8 name/anim hash) + cartesian-product preimage search.
// Port of work/lukzb-fork/kamuiHash.py (checked against it by kamui_search.py --selftest).
//
// usage: kamui_search targets.txt list1.txt [list2.txt ...]
//   targets.txt: one "HEXHASH LEN" per line (LEN 0 = unknown length)
//   listN.txt:   one token per line (may be empty line = empty token); the candidate is
//                the concatenation of one token from each list, in order.
// usage: kamui_search --hash   (reads strings on stdin, prints "HASH string")
// Prints "HASH\tcandidate" for each hit on stdout.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define C1 0xCC9E2D51u
#define C2 0x1B873593u
#define MIX_SUB 0x052250ECu
#define MIX_SUB2 0x19AB949Cu
#define MIX_MUL1 0x3361D2AFu
#define FMIX1 0x85EBCA6Bu
#define FMIX2 0xC2B2AE35u

static inline uint32_t rol(uint32_t v, int c) { c &= 31; return c ? (v << c) | (v >> (32 - c)) : v; }
static inline uint32_t ror(uint32_t v, int c) { c &= 31; return c ? (v >> c) | (v << (32 - c)) : v; }
static inline uint32_t rd(const uint8_t *d, int o) { uint32_t v; memcpy(&v, d + o, 4); return v; }
static inline uint32_t bswap(uint32_t v) { return __builtin_bswap32(v); }
static inline uint32_t mix(uint32_t k) { k *= C1; k = rol(k, 15); return k * C2; }

static uint32_t h12_24(const uint8_t *d, uint32_t n) {
    uint32_t mid = n >> 1;
    uint32_t k1 = mix(rd(d, n - 4)), k2 = mix(rd(d, 0)), k3 = mix(rd(d, mid));
    uint32_t k4 = mix(rd(d, n - 8)), k5 = mix(rd(d, 4)), k6 = mix(rd(d, mid - 4));
    uint32_t h = n;
    h ^= k6; h = 5 * (rol(h, 13) - MIX_SUB);
    h ^= k5; h = 5 * rol(h, 13) - MIX_SUB2;
    h ^= k4; h = 5 * (rol(h, 13) - MIX_SUB);
    h ^= k3; h = 5 * rol(h, 13) - MIX_SUB2;
    h ^= k2; h = 5 * (rol(h, 13) - MIX_SUB);
    h ^= k1; h = rol(h, 13);
    uint32_t f = 5 * (h - MIX_SUB);
    f ^= f >> 16; f *= FMIX1; f ^= f >> 13; f *= FMIX2;
    return f ^ (f >> 16);
}

static uint32_t kamui(const uint8_t *d, uint32_t n) {
    if (n > 24) {
        uint32_t k4 = mix(rd(d, n - 4)), k8 = mix(rd(d, n - 8)), k12 = mix(rd(d, n - 12));
        uint32_t k16 = mix(rd(d, n - 16)), k20 = mix(rd(d, n - 20));
        uint32_t h1 = 5 * (rol(n ^ k4, 13) - MIX_SUB);
        uint32_t v14 = 5 * (rol(k16 ^ h1, 13) - MIX_SUB);
        uint32_t h2 = 5 * (rol((C1 * n) ^ k8, 13) - MIX_SUB);
        uint32_t v15 = 5 * (rol(k12 ^ h2, 13) - MIX_SUB);
        uint32_t v16 = 5 * (rol(C1 * n + k20, 13) - MIX_SUB);
        uint32_t it = (n - 1) / 20; int cur = 6;
        for (uint32_t i = 0; i < it; i++) {
            uint32_t a = mix(rd(d, cur - 6)), b = rd(d, cur - 2), c = mix(rd(d, cur + 2));
            uint32_t dd = mix(rd(d, cur + 6)), e = rd(d, cur + 10);
            uint32_t n14 = a - MIX_MUL1 * rol(b + v16, 13);
            uint32_t t1 = rol(v14 ^ a, 14);
            uint32_t t2 = (5 * (t1 - MIX_SUB)) ^ (b + dd);
            uint32_t n15 = bswap(5 * (e + rol(t2, 13) - MIX_SUB));
            uint32_t t3 = rol(v15 + c, 14);
            uint32_t n16 = 5 * bswap(e ^ (5 * (t3 - MIX_SUB)));
            v14 = n14; v15 = n15; v16 = n16; cur += 20;
        }
        uint32_t fm = rol(C1 * ror(v16, 11), 15);
        uint32_t inner = v14 - MIX_MUL1 * rol(C1 * ror(v15, 11), 15);
        uint32_t fh = 5 * rol(C1 * (fm + rol(5 * (rol(inner, 13) - MIX_SUB), 15)), 13) - MIX_SUB2;
        return C1 * rol(fh, 15);
    }
    if (n > 12) return h12_24(d, n);
    if (n > 4) {
        uint32_t ks = mix(n + rd(d, 0));
        uint32_t h = 5 * (rol((5 * n) ^ ks, 13) - MIX_SUB);
        uint32_t ke = mix(5 * n + rd(d, n - 4));
        h = 5 * (rol(h ^ ke, 13) - MIX_SUB);
        uint32_t km = C2 * rol(0x318F97D9u - MIX_MUL1 * rd(d, (n >> 1) & 4), 15);
        uint32_t v = rol(h ^ km, 13), t = 5 * (v - MIX_SUB);
        uint32_t f = FMIX1 * (t ^ (t >> 16)), r = FMIX2 * (f ^ (f >> 13));
        return r ^ (r >> 16);
    }
    uint32_t acc = 0, x = 9;
    for (uint32_t i = 0; i < n; i++) { acc = (uint32_t)(int32_t)(int8_t)d[i] - MIX_MUL1 * acc; x ^= acc; }
    uint32_t v = rol((5 * (rol(mix(n) ^ x, 13) - MIX_SUB)) ^ mix(acc), 13), t = 5 * (v - MIX_SUB);
    uint32_t f = FMIX1 * (t ^ (t >> 16)), r = FMIX2 * (f ^ (f >> 13));
    return r ^ (r >> 16);
}

// --- target set: open-addressing hash table ---
#define TB (1u << 16)
static uint32_t th[TB]; static uint16_t tl[TB]; static uint8_t tu[TB];
static void tadd(uint32_t h, int len) { uint32_t i = (h * 2654435761u) >> 16; while (tu[i]) i = (i + 1) & (TB - 1); th[i] = h; tl[i] = len; tu[i] = 1; }
static inline int tfind(uint32_t h, int len) {
    uint32_t i = (h * 2654435761u) >> 16;
    while (tu[i]) { if (th[i] == h && (tl[i] == 0 || tl[i] == len)) return 1; i = (i + 1) & (TB - 1); }
    return 0;
}

typedef struct { char **s; int *l; int n; } List;
static List load(const char *p) {
    FILE *f = fopen(p, "rb"); if (!f) { perror(p); exit(1); }
    List L = {0}; int cap = 0; char buf[512];
    while (fgets(buf, sizeof buf, f)) {
        int k = (int)strlen(buf); while (k && (buf[k-1] == '\n' || buf[k-1] == '\r')) buf[--k] = 0;
        if (L.n == cap) { cap = cap ? cap * 2 : 64; L.s = realloc(L.s, cap * sizeof(char*)); L.l = realloc(L.l, cap * sizeof(int)); }
        L.s[L.n] = strdup(buf); L.l[L.n] = k; L.n++;
    }
    fclose(f); return L;
}

static List *lists; static int nl; static uint8_t cand[1024]; static unsigned long long tried, hits;
static void rec(int depth, int pos) {
    if (depth == nl) {
        tried++;
        uint32_t h = kamui(cand, pos);
        if (tfind(h, pos)) { hits++; cand[pos] = 0; printf("%08X\t%s\n", h, cand); }
        return;
    }
    List *L = &lists[depth];
    for (int i = 0; i < L->n; i++) {
        if (pos + L->l[i] > 1000) continue;
        memcpy(cand + pos, L->s[i], L->l[i]);
        rec(depth + 1, pos + L->l[i]);
    }
}

int main(int argc, char **argv) {
    if (argc >= 2 && !strcmp(argv[1], "--hash")) {
        char buf[1100];
        while (fgets(buf, sizeof buf, stdin)) {
            int k = (int)strlen(buf); while (k && (buf[k-1] == '\n' || buf[k-1] == '\r')) buf[--k] = 0;
            uint8_t tmp[1100] = {0}; memcpy(tmp, buf, k);
            printf("%08X %s\n", kamui(tmp, k), buf);
        }
        return 0;
    }
    if (argc < 3) { fprintf(stderr, "usage: %s targets.txt list1 [list2...] | --hash\n", argv[0]); return 1; }
    FILE *f = fopen(argv[1], "r"); if (!f) { perror(argv[1]); return 1; }
    unsigned h; int len; int nt = 0;
    while (fscanf(f, "%x %d", &h, &len) == 2) { tadd(h, len); nt++; }
    fclose(f);
    nl = argc - 2; lists = calloc(nl, sizeof(List));
    for (int i = 0; i < nl; i++) lists[i] = load(argv[i + 2]);
    memset(cand, 0, sizeof cand);
    rec(0, 0);
    fprintf(stderr, "targets %d tried %llu hits %llu\n", nt, tried, hits);
    return 0;
}
