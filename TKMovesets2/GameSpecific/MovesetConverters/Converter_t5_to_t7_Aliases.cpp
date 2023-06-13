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
 std::map<unsigned int, s_propAlias> MovesetConverter::T5ToT7::InitAliasDictionary()
{
	return std::map<unsigned int, s_propAlias> {
        { 2, { .target_id = 3 }},
        { 3, {.target_id = 4 } },
        { 4, {.target_id = 5 } },
        { 5, {.target_id = 6 } },
        { 6, {.target_id = 7 } },
        { 7, {.target_id = 8 } },
        { 8, {.target_id = 9 } },
        { 9, {.target_id = 10 } },
        { 10, {.target_id = 11 } },
        { 11, {.target_id = 12 } },
        { 12, {.target_id = 13 } },
        { 13, {.target_id = 14 } },
        { 14, {.target_id = 15 } },
        { 15, {.target_id = 16 } },
        { 16, {.target_id = 17 } },
        { 17, {.target_id = 18 } },
        { 24, {.target_id = 27 } },
        { 25, {.target_id = 28 } },
        { 26, {.target_id = 29 } },
        { 27, {.target_id = 30 } },
        { 32, {.target_id = 35 } },
        { 33, {.target_id = 36 } },
        { 40, {.target_id = 44 } },
        { 41, {.target_id = 47 } },
        { 48, {.target_id = 66 } },
        { 49, {.target_id = 67 } },
        { 50, {.target_id = 68 } },
        { 51, {.target_id = 69 } },
        { 52, {.target_id = 70 } },
        { 53, {.target_id = 71 } },
        { 55, {.target_id = 72 } },
        { 56, {.target_id = 73 } },
        { 63, {.target_id = 80 } },
        { 64, {.target_id = 81 } },
        { 65, {.target_id = 82 } },
        { 66, {.target_id = 83 } },
        { 67, {.target_id = 84 } },
        { 68, {.target_id = 85 } },
        { 69, {.target_id = 86 } },
        { 70, {.target_id = 87 } },
        { 79, {.target_id = 103 } },
        { 97, {.target_id = 124 } },
        { 98, {.target_id = 125 } },
        { 99, {.target_id = 126 } },
        { 100, {.target_id = 127 } },
        { 101, {.target_id = 128 } },
        { 102, {.target_id = 129 } },
        { 103, {.target_id = 130 } },
        { 104, {.target_id = 131 } },
        { 105, {.target_id = 132 } },
        { 106, {.target_id = 133 } },
        { 107, {.target_id = 134 } },
        { 108, {.target_id = 135 } },
        { 111, {.target_id = 146 } },
        { 112, {.target_id = 147 } },
        { 113, {.target_id = 148 } },
        { 114, {.target_id = 149 } },
        { 115, {.target_id = 150 } },
        { 125, {.target_id = 167 } },
        { 133, {.target_id = 182 } },
        { 140, {.target_id = 216 } },
        { 141, {.target_id = 217 } },
        { 142, {.target_id = 218 } },
        { 143, {.target_id = 219 } },
        { 144, {.target_id = 220 } },
        { 145, {.target_id = 221 } },
        { 146, {.target_id = 222 } },
        { 147, {.target_id = 223 } },
        { 148, {.target_id = 224 } },
        { 149, {.target_id = 225 } },
        { 172, {.target_id = 289 } },
        { 184, {.target_id = 352 } },
        { 280, {.target_id = 614 } },
        { 321, {.target_id = 881 } },
        { 0x8001, {.target_id = 0x8001 } },
        { 0x8002, {.target_id = 0x8003 } },
        { 0x8003, {.target_id = 0x8004 } },
        { 0x8004, {.target_id = 0x8006 } },
        { 0x8006, {.target_id = 0x8008 } },
        { 0x8007, {.target_id = 0x8009 } },
        { 0x8019, {.target_id = 0x802e } },
        { 0x804b, {.target_id = 0x80a6 } },
        { 0x8067, {.target_id = 0x817d } },
        { 0x806f, {.target_id = 0x817d } },
        { 0x8073, {.target_id = 0x8188 } },
        { 0x8076, {.target_id = 0x818d } },
        { 0x8077, {.target_id = 0x818e } },
        { 0x8078, {.target_id = 0x8193 } },
        { 0x807d, {.target_id = 0x8198 } },
        { 0x807f, {.target_id = 0x8199 } },
        { 0x8081, {.target_id = 0x81a3 } },
        { 0x8082, {.target_id = 0x81a4 } },
        { 0x80e3, {.target_id = 0x853d } },
        { 0x80e4, {.target_id = 0x824c } },
        { 0x813e, {.target_id = 0x8428 } },
        { 0x813f, {.target_id = 0x8429 } },
        { 0x8140, {.target_id = 0x842a } },
        { 0x8141, {.target_id = 0x842c } },
        { 0x8142, {.target_id = 0x842d } },
        { 0x8143, {.target_id = 0x842e } },
        { 0x8144, {.target_id = 0x842f } },
        { 0x8147, {.target_id = 0x8435 } },
        { 0x814c, {.target_id = 0x84c2 } },
        { 0x814e, {.target_id = 0x84c4 } },
        { 0x814f, {.target_id = 0x84c6 } },
        { 0x8152, {.target_id = 0x84cb } },
        { 0x8153, {.target_id = 0x84cc } },
	};
}

namespace T5_T7_Aliases
{
	int ConvertMove0x98(int value)
	{
        if (value == 0x20000000) return 0x04000000;
        return value;
	}

    uint32_t ConvertVoiceclip(uint16_t value)
    {
        // Converts 2 byte value into 4 byte equivalent
        if (value == 0xFFFF) {
            return 0xFFFFFFFF;
        }
        else if (0x0F00 <= value && value < 0x1000) {
            // If in range of 0x0F00
            return (value << 16) & 0xFF000000 | (value & 0x000000FF);
        }
        else {
            //E.g; 0x2006 -> 0x0200006
            return (value << 12) & 0xFF000000 | value & 0x000000FF;
        }
    }

    uint64_t ConvertCommand(uint32_t command)
    {
        uint64_t t = 0;
        uint64_t c = (uint64_t)command;

        t = (c & 0x00FF0000) << 16;
        t |= (c & 0xFF000000) << 33;
        t |= (c & 0x0000FFFF);

        switch (t)
        {
        case 0x8005:
            // Group cancel
            return 0x800b;
        case 0x8006:
            // Group cancel END
            return 0x800c;
        default:
            c = t;
            if (0x8013 <= t && t <= 0x81FF) {
                // Input sequences
                c += 6;
            }
            return c;
       }
    }
};
