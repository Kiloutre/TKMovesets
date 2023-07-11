#pragma once

#include <map>

#include "MovesetConverters.hpp"

#include "GameTypes.h"

const auto cg_odd_hitbox_aliases = std::map<Byte, Byte>{
	{ 0x26, 0x2f }, // Leg hitbox
	{ 0x25, 0x2e }, // Leg 
	{ 0x24, 0x2c }, // Leg 
	{ 0x42, 0x52 }, // Alisa DES2
	{ 0x41, 0x51 }, // Alisa DES F1
	//{ 0x44, 0x09 }, // Kunimitsu b+2 right arm, tofix properly
	//{ 0x45, 0x09 }, // Kunimitsu doton 2 right arm, tofix properly
	{ 0x40, 0x50 }, // DJ laser
	{ 0x1E, 0x50 }, // Kunimitsu fire breath
};

const auto cg_even_hitbox_aliases = std::map<Byte, Byte>{
	{ 0x1E, 0x16 }, // Kunimitsu fire breath
};

// Aliases for extraprops andrequirements
// Will be built from cg_propertyAliases, attempts to guess unknown values in between known values
 std::map<unsigned int, s_propAlias> MovesetConverter::T6ToT7::InitAliasDictionary()
{
	return std::map<unsigned int, s_propAlias> {
        { 0, { .target_id = 0x8261 }},
        { 44, {.target_id = 47 } },
        { 45, {.target_id = 47 } },
        { 46, {.target_id = 48 } },
        { 54, {.target_id = 66 } },
        { 61, {.target_id = 72 } },
        { 104, {.target_id = 122 } },
        { 112, {.target_id = 130 } },
        { 117, {.target_id = 135 } },
        { 128, {.target_id = 146 } },
        { 142, {.target_id = 135 } },
        { 143, {.target_id = 182 } },
        { 152, {.target_id = 216 } },
        { 194, {.target_id = 283 } },
        { 209, {.target_id = 329 } },
        { 214, {.target_id = 352 } },
        { 236, {.target_id = 533 } },
        { 261, {.target_id = 552 } },
        { 297, {.target_id = 598 } },
        { 310, {.target_id = 604 } },
        { 323, {.target_id = 614 } },
        { 324, {.target_id = 615 } },
        { 346, {.target_id = 135 } },
        { 397, {.target_id = 881 } },
        { 0x8001, {.target_id = 0x8001 } },
        { 0x8002, {.target_id = 0x8002 } },
        { 0x8003, {.target_id = 0x8003 } },
        { 0x8004, {.target_id = 0x8004 } },
        { 0x8007, {.target_id = 0x8007 } },
        { 0x8008, {.target_id = 0x8008 } },
        { 0x8009, {.target_id = 0x8009 } },
        { 0x800b, {.target_id = 0x800b } },
        { 0x800c, {.target_id = 0x800c } },
        { 0x800f, {.target_id = 0x800e } },
        { 0x8011, {.target_id = 0x8011 } },
        { 0x8014, {.target_id = 0x8011 } },
        { 0x801c, {.target_id = 0x801e } },
        { 0x801d, {.target_id = 0x801f } },
        { 0x801e, {.target_id = 0x802e } },
        { 0x8020, {.target_id = 0x8036 } },
        { 0x8021, {.target_id = 0x8039 } },
        { 0x8023, {.target_id = 0x803b } },
        { 0x8025, {.target_id = 0x8042 } },
        { 0x8026, {.target_id = 0x8043 } },
        { 0x8027, {.target_id = 0x8044 } },
        { 0x802a, {.target_id = 0x8046 } },
        { 0x802d, {.target_id = 0x8048 } },
        { 0x803b, {.target_id = 0x8058 } },
        { 0x803c, {.target_id = 0x805a } },
        { 0x804f, {.target_id = 0x821c } },
        { 0x8050, {.target_id = 0x821b } },
        { 0x8053, {.target_id = 0x806e } },
        { 0x8055, {.target_id = 0x8074 } },
        { 0x8058, {.target_id = 0x8078 } },
        { 0x805c, {.target_id = 0x807c } },
        { 0x8068, {.target_id = 0x80a6 } },
        { 0x8070, {.target_id = 0x80d4 } },
        { 0x8077, {.target_id = 0x80db } },
        { 0x8078, {.target_id = 0x80dc } },
        { 0x8083, {.target_id = 0x817c } },
        { 0x8084, {.target_id = 0x817d } },
        { 0x8085, {.target_id = 0x817e } },
        { 0x8086, {.target_id = 0x817f } },
        { 0x8087, {.target_id = 0x8180 } },
        { 0x8088, {.target_id = 0x8181 } },
        { 0x808a, {.target_id = 0x8185 } },
        { 0x808d, {.target_id = 0x8188 } },
        { 0x808e, {.target_id = 0x8189 } },
        { 0x808f, {.target_id = 0x818a } },
        { 0x8091, {.target_id = 0x818c } },
        { 0x8094, {.target_id = 0x818d } },
        { 0x8095, {.target_id = 0x818e } },
        { 0x8096, {.target_id = 0x8190 } },
        { 0x8097, {.target_id = 0x8193 } },
        { 0x8099, {.target_id = 0x8195 } },
        { 0x809a, {.target_id = 0x8196 } },
        { 0x809c, {.target_id = 0x8198 } },
        { 0x809e, {.target_id = 0x8199 } },
        { 0x80a0, {.target_id = 0x8188 } },
        { 0x80a1, {.target_id = 0x81a3 } },
        { 0x80a2, {.target_id = 0x81a4 } },
        { 0x80a4, {.target_id = 0x81a6 } },
        { 0x80a5, {.target_id = 0x81a7 } },
        { 0x80a7, {.target_id = 0x81a9 } },
        { 0x80a8, {.target_id = 0x81aa } },
        { 0x80aa, {.target_id = 0x81ac } },
        { 0x80ab, {.target_id = 0x81c8 } },
        { 0x80ba, {.target_id = 0x81d6 } },
        { 0x80bf, {.target_id = 0x81db } },
        { 0x80c1, {.target_id = 0x81dd } },
        { 0x80c3, {.target_id = 0x81df } },
        { 0x80c4, {.target_id = 0x81e0 } },
        { 0x80c6, {.target_id = 0x81e3 } },
        { 0x80c7, {.target_id = 0x81e4 } },
        { 0x80c8, {.target_id = 0x81e5 } },
        { 0x80c9, {.target_id = 0x81e6 } },
        { 0x80cb, {.target_id = 0x81e8 } },
        { 0x80cc, {.target_id = 0x81e9 } },
        { 0x80cd, {.target_id = 0x81ea } },
        { 0x80ce, {.target_id = 0x81eb } },
        { 0x80cf, {.target_id = 0x81ec } },
        { 0x80d2, {.target_id = 0x81ef } },
        { 0x80d3, {.target_id = 0x81f0 } },
        { 0x80d5, {.target_id = 0x81f2 } },
        { 0x80d7, {.target_id = 0x81f4 } },
        { 0x80d9, {.target_id = 0x81f9 } },
        { 0x80de, {.target_id = 0x81fe } },
        { 0x80e4, {.target_id = 0x8207 } },
        { 0x80e6, {.target_id = 0x82ab } },
        { 0x80e7, {.target_id = 0x820a } },
        { 0x8107, {.target_id = 0x8245 } },
        { 0x8109, {.target_id = 0x8246 } },
        { 0x810c, {.target_id = 0x8249 } },
        { 0x810e, {.target_id = 0x824b } },
        { 0x810f, {.target_id = 0x824c } },
        { 0x8111, {.target_id = 0x824d } },
        { 0x8112, {.target_id = 0x824e } },
        { 0x8120, {.target_id = 0x8255 } },
        { 0x8123, {.target_id = 0x8257 } },
        { 0x8148, {.target_id = 0x826a } },
        { 0x8149, {.target_id = 0x826b } },
        { 0x814a, {.target_id = 0x826c } },
        { 0x814b, {.target_id = 0x826d } },
        { 0x8150, {.target_id = 0x8188 } },
        { 0x8151, {.target_id = 0x82d6 } },
        { 0x8152, {.target_id = 0x8207 } },
        { 0x8153, {.target_id = 0x818a } },
        { 0x8154, {.target_id = 0x8193 } },
        { 0x8163, {.target_id = 0x8387 } },
        { 0x8164, {.target_id = 0x8388 } },
        { 0x8165, {.target_id = 0x8389 } },
        { 0x8166, {.target_id = 0x838a } },
        { 0x8167, {.target_id = 0x838b } },
        { 0x8168, {.target_id = 0x838c } },
        { 0x816a, {.target_id = 0x838e } },
        { 0x816b, {.target_id = 0x838f } },
        { 0x816c, {.target_id = 0x8390 } },
        { 0x816e, {.target_id = 0x8392 } },
        { 0x81c2, {.target_id = 0x8428 } },
        { 0x81c3, {.target_id = 0x8429 } },
        { 0x81c4, {.target_id = 0x842a } },
        { 0x81c5, {.target_id = 0x842c } },
        { 0x81cf, {.target_id = 0x843c } },
        { 0x81e3, {.target_id = 0x84c3 } },
        { 0x81e4, {.target_id = 0x84c4 } },
        { 0x81e5, {.target_id = 0x84c6 } },
        { 0x81e6, {.target_id = 0x84c8 } },
        { 0x81e8, {.target_id = 0x84cc } },
        { 0x8202, {.target_id = 0x853c } },
        { 0x8203, {.target_id = 0x853d } },
	};
}

namespace T6_T7_Aliases
{
	Byte OddHitboxByte(Byte value)
	{
		auto item = cg_odd_hitbox_aliases.find(value);
		if (item != cg_odd_hitbox_aliases.end()) {
			return item->second;
		}
		return value;
	}

	Byte EvenHitboxByte(Byte value)
	{
		auto item = cg_even_hitbox_aliases.find(value);
		if (item != cg_even_hitbox_aliases.end()) {
			return item->second;
		}
		return value;
	}

	int ConvertMove0x98(int value)
	{
		// Bit reversal except on the last bit
		int tmp = 0;
		for (unsigned int i = 0; i < 7; ++i)
		{
            if ((value & (1 << i)) != 0) {
                tmp |= 1 << (7 - i);
            }
		}
		return (value >> 7) | (tmp << 24);
	}

	void ApplyHitboxAlias(unsigned int& hitbox)
	{
		Byte* orig_hitbox_bytes = (Byte*)&hitbox;

		for (unsigned int i = 0; i < 4; ++i)
		{
			Byte b = orig_hitbox_bytes[i];
			orig_hitbox_bytes[i] = (b & 1) ? OddHitboxByte(b) : EvenHitboxByte(b);
		}
	}
};
