#pragma once
#include <bit>
#include <cstdint>
#include <string_view>
#include <cstring>
#include <optional>

namespace RetailNames {
// Retail Kamui name hash. Arithmetic is intentionally modulo 2^32; strings
// are byte sequences, with signed bytes in the short-string branch.
inline uint32_t Hash(std::string_view text) {
    const auto n = static_cast<uint32_t>(text.size());
    auto word = [&](size_t at) { uint32_t v; memcpy(&v,text.data()+at,4); return v; };
    auto mix = [](uint32_t v) { return std::rotl(v*0xcc9e2d51U,15)*0x1b873593U; };
    auto step = [](uint32_t v) { return 5U*(std::rotl(v,13)-0x052250ecU); };
    auto finish = [](uint32_t v) { v=(v^(v>>16))*0x85ebca6bU; v=(v^(v>>13))*0xc2b2ae35U; return v^(v>>16); };
    auto swap = [](uint32_t v) { return (v>>24)|((v>>8)&0xff00U)|((v<<8)&0xff0000U)|(v<<24); };
    if (n > 24) {
        uint32_t a=step(mix(word(n-16))^step(n^mix(word(n-4))));
        uint32_t b=step(mix(word(n-12))^step(n*0xcc9e2d51U^mix(word(n-8))));
        uint32_t c=step(n*0xcc9e2d51U+mix(word(n-20)));
        for(size_t at=0,i=0;i<(n-1)/20;++i,at+=20) {
            const auto w=word(at+4), x=word(at+16), m=mix(word(at));
            const auto nextA=m-0x3361d2afU*std::rotl(w+c,13);
            const auto nextB=swap(step(5U*(std::rotl(a^m,14)-0x052250ecU)^(w+mix(word(at+12))))+5U*x);
            c=5U*swap(x^5U*(std::rotl(b+mix(word(at+8)),14)-0x052250ecU));
            a=nextA; b=nextB;
        }
        const auto inner=a-0x3361d2afU*std::rotl(0xcc9e2d51U*std::rotr(b,11),15);
        const auto h=step(0xcc9e2d51U*(std::rotl(0xcc9e2d51U*std::rotr(c,11),15)+std::rotl(step(inner),15)));
        return 0xcc9e2d51U*std::rotl(h,15);
    }
    if(n>12) {
        uint32_t h=n;
        for(auto at:{(n>>1)-4,4U,n-8,n>>1,0U,n-4}) h=step(h^mix(word(at)));
        return finish(h);
    }
    if(n>4) {
        const auto h=step(step(5U*n^mix(n+word(0)))^mix(5U*n+word(n-4)));
        const auto m=0x1b873593U*std::rotl(0x318f97d9U-0x3361d2afU*word((n>>1)&4),15);
        return finish(step(h^m));
    }
    uint32_t a=0,x=9;
    for(unsigned char ch:text) { a=static_cast<uint32_t>(static_cast<int8_t>(ch))-0x3361d2afU*a; x^=a; }
    return finish(step(step(mix(n)^x)^mix(a)));
}
// Decode an encrypted retail field {value, key} (move fields: name hash at 0x00, animation hash at 0x20).
inline std::optional<uint32_t> Decode(uint64_t value, uint64_t key) {
    uint32_t check = 0, x = static_cast<uint32_t>(value);
    for (int shift = 0; shift < 32; shift += 8, x >>= 8) check ^= x ^ static_cast<uint32_t>(std::rotl(key, shift + 8));
    if (!check) check = 1;
    if (static_cast<uint32_t>(value >> 32) != check) return {};
    auto s = value ^ 0x1dULL;
    return static_cast<uint32_t>((std::rotl(key, static_cast<int>(s & 31)) & ~31ULL) ^ s);
}
inline uint64_t Encode(uint32_t value) {
    const auto low=value^0x1dU;
    const auto check=low^(low>>8)^(low>>16)^(low>>24);
    return low|(uint64_t(check?check:1)<<32); // zero encryption key
}
}
