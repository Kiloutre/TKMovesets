#include "Animations.hpp"
#include "AnimationReader.hpp"
#include <array>
#include <cmath>
#include <vector>
#include <stdint.h>
#include <cstring>

// ============================= //

typedef unsigned char Byte;

// ============================= //

template<class T> static T ReadUnaligned(const Byte* address)
{
    T value;
    memcpy(&value, address, sizeof(value));
    return value;
}
#define READ(type, addr) ReadUnaligned<type>(addr)

struct s_weirdStack {
	const Byte* anim_ptr;
	const Byte* anim_end;
	uint32_t remainder;
	int starts_at_two;
};

const float mult_const = 0.000095873802f; // pi / 32769

// ============================= //

// Bone descriptors between 4 and 7 indicate 3x short type. Otherwise, it's a 3x float type.
static inline bool isBoneShort(uint16_t bone_descriptor)
{
	return (4 <= bone_descriptor && bone_descriptor <= 7);
}


// I have no idea
static int16_t sub_1407316F0(s_weirdStack* weirdStack, int a2)
{
	int starts_at_two;
	unsigned int remainder;

	starts_at_two = weirdStack->starts_at_two;
	remainder = weirdStack->remainder;
	if (starts_at_two < a2)
	{
		unsigned int iter_count = ((unsigned int)(a2 - starts_at_two - 1) >> 3) + 1;
		starts_at_two += 8 * iter_count;

		for (unsigned int i = 0; i < iter_count; ++i)
		{
			if (weirdStack->anim_ptr == weirdStack->anim_end) throw std::out_of_range("Animation channel is truncated");
			auto value = *weirdStack->anim_ptr;
			remainder = value | (remainder << 8);

			++weirdStack->anim_ptr;
		}
	}

	int v7 = starts_at_two - a2;
	weirdStack->starts_at_two = v7;
	weirdStack->remainder = remainder & ((1u << v7) - 1);

	return remainder >> v7;
}

// I have no idea
static int TK__Anim_64_sub_140733340(s_weirdStack* weirdStack, uint16_t* a2a_short, int* __x1_ptr, signed int* __x2_ptr)
{
	int retVal;
	int new_x1;
	int new_x2;

	if (*a2a_short)
	{
		*a2a_short -= 1;

		new_x1 = *__x1_ptr;
		new_x2 = *__x2_ptr;

		if (new_x1)
		{
			if (new_x1 == 99) {
				retVal = 0xF0000000;
			}
			else {
				retVal = new_x2 + sub_1407316F0(weirdStack, new_x1);
			}
		}
		else {
			retVal = 0;
		}
	}
	else
	{
		switch (sub_1407316F0(weirdStack, 4))
		{
		case 0:
			new_x1 = 0;
			new_x2 = 0;
			retVal = 0;
			break;

		case 1:
			new_x1 = 2;
			new_x2 = 1;
			retVal = new_x2 + sub_1407316F0(weirdStack, new_x1);
			break;

		case 2:
			new_x1 = 2;
			new_x2 = -4;
			retVal = sub_1407316F0(weirdStack, 2) - 4;
			break;

		case 3:
			new_x1 = 4;
			new_x2 = 5;
			retVal = sub_1407316F0(weirdStack, 4) + 5;
			break;

		case 4:
			new_x1 = 4;
			new_x2 = -20;
			retVal = sub_1407316F0(weirdStack, 4) - 20;
			break;

		case 5:
			new_x1 = 6;
			new_x2 = 21;
			retVal = sub_1407316F0(weirdStack, 6) + 21;
			break;

		case 6:
			new_x1 = 6;
			new_x2 = -84;
			retVal = sub_1407316F0(weirdStack, 6) - 84;
			break;

		case 7:
			new_x1 = 8;
			new_x2 = 85;
			retVal = sub_1407316F0(weirdStack, 8) + 85;
			break;

		case 8:
			new_x1 = 8;
			new_x2 = -340;
			retVal = sub_1407316F0(weirdStack, 8) - 340;
			break;

		case 9:
			new_x1 = 10;
			new_x2 = 341;
			retVal = sub_1407316F0(weirdStack, 10) + 341;
			break;

		case 10:
			new_x1 = 10;
			new_x2 = -1364;
			retVal = sub_1407316F0(weirdStack, 10) - 1364;
			break;

		case 11:
			new_x1 = 12;
			new_x2 = 1365;
			retVal = sub_1407316F0(weirdStack, 12) + 1365;
			break;

		case 12:
			new_x1 = 12;
			new_x2 = -5460;
			retVal = sub_1407316F0(weirdStack, 12) - 5460;
			break;

		case 13:
			new_x1 = 16;
			new_x2 = 0;
			retVal = sub_1407316F0(weirdStack, 16);
			break;

		case 14:
			*a2a_short = sub_1407316F0(weirdStack, 4) - 1;

			new_x1 = *__x1_ptr;
			new_x2 = *__x2_ptr;

			if (new_x1)
			{
				if (new_x1 == 99) {
					retVal = 0xF0000000;
				}
				else {
					retVal = new_x2 + sub_1407316F0(weirdStack, new_x1);
				}
			}
			else {
				retVal = 0;
			}

			break;

		case 15:
			new_x1 = 99;
			new_x2 = 0;
			retVal = -268435456;
			break;

		default:
			// This really should never happen and is a sign of a bad animation or a bad algorithm
			throw std::invalid_argument("Invalid animation channel opcode");
			break;
		}
	}

	*__x1_ptr = new_x1;
	*__x2_ptr = new_x2;
	return retVal;
}


// Parse the bone value, detecting if it is a short or float and filling the buffers accordingly
// Returns 6 (3x sizeof uint) or 12 (3x sizeof float) depending on the bone's data type
static unsigned int StoreBaseBoneValue(uint16_t bone_descriptor, const Byte* __anim_ptr, float* bone_value, int16_t* v81_buf_ptr, int short_value_shift)
{
	if (isBoneShort(bone_descriptor))
	{
		// Is short
		int16_t values[3] = {
			READ(int16_t, __anim_ptr + 0),
			READ(int16_t, __anim_ptr + 2),
			READ(int16_t, __anim_ptr + 4),
		};

		bone_value[0] = (float)values[0] * mult_const;
		bone_value[1] = (float)values[1] * mult_const;
		bone_value[2] = (float)values[2] * mult_const;

		// Values will be shifted back (<<) by the same amount When reading the animation data in the targeted frame later
		v81_buf_ptr[0] = values[0] >> short_value_shift;
		v81_buf_ptr[1] = values[1] >> short_value_shift;
		v81_buf_ptr[2] = values[2] >> short_value_shift;

		return sizeof(uint16_t) * 3;
	}
	else
	{
		// Is float

		bone_value[0] = READ(float, __anim_ptr);
		bone_value[1] = READ(float, __anim_ptr + 4);
		bone_value[2] = READ(float, __anim_ptr + 8);

		v81_buf_ptr[0] = 0;
		v81_buf_ptr[1] = 0;
		v81_buf_ptr[2] = 0;

		return sizeof(float) * 3;
	}
}



namespace TAnimUtils::FromMemory
{
uint16_t ParseAnimation0x64(std::span<const Byte> bytes, unsigned int frame, std::span<float> output)
{
    try {
        auto layout = ReadAnimationLayout(bytes);
        if (layout.type != 0x64 || !frame || output.size() < layout.bones * 3)
            return 0;
        std::vector<Byte> normalized;
        if (layout.swapped) {
            normalized.assign(bytes.begin(), bytes.begin() + layout.size);
            if (!ByteswapAnimation(normalized.data(), normalized.size())) return 0;
            bytes = normalized;
            layout = ReadAnimationLayout(bytes);
        }
        AnimationReader input(bytes);
        const Byte* anim = bytes.data();
        const auto shortShift = input.Read<Byte>(layout.postDescriptors + 2) & 0x7f;
        const auto floatShift = input.Read<Byte>(layout.postDescriptors + 3);
        if (shortShift > 15 || floatShift > 31) return 0;
        std::array<float, 255 * 3> positions{};
        std::array<float, 255> floatOffsets{};
        std::array<int16_t, 255 * 3> values{};
        for (size_t i = 0; i < layout.floatBones; ++i)
            floatOffsets[i] = input.Read<float>(layout.postDescriptors + 6 + i * 4);
        size_t pose = layout.basePose;
        for (size_t i = 0; i < layout.bones; ++i) {
            auto descriptor = input.Read<uint16_t>(4 + i * 2);
            pose += StoreBaseBoneValue(descriptor, anim + pose, positions.data() + i * 3,
                values.data() + i * 3, shortShift);
        }
        frame = (std::min)(frame, layout.frames);
        if (frame > 1) {
            const unsigned baseFrame = frame - 2;
            const unsigned subframe = baseFrame & 15;
            const size_t keyframe = baseFrame / 16;
            const size_t chunks = (layout.frames + 14) / 16;
            if (keyframe >= chunks) return 0;
            size_t cursor = layout.keyframes + input.Read<uint32_t>(layout.keyframes + keyframe * 4);
            size_t floatBone = 0;
            int width = 0, bias = 0;
            for (size_t bone = 0; bone < layout.bones; ++bone) {
                auto descriptor = input.Read<uint16_t>(4 + bone * 2);
                for (size_t axis = 0; axis < 3; ++axis) {
                    const auto prefix = input.Read<Byte>(cursor);
                    const size_t length = prefix >> 2;
                    input.Require(cursor, length);
                    if (!length) return 0;
                    s_weirdStack state{anim + cursor + 1, anim + cursor + length,
                        static_cast<uint32_t>(prefix & 3), 2};
                    uint16_t repeat = 0;
                    const size_t index = bone * 3 + axis;
                    values[index] = static_cast<int16_t>(static_cast<uint16_t>(values[index]) +
                        TK__Anim_64_sub_140733340(&state, &repeat, &width, &bias));
                    int sum = 0;
                    for (unsigned step = 0; step < subframe; ++step) {
                        int delta = TK__Anim_64_sub_140733340(&state, &repeat, &width, &bias);
                        if (static_cast<uint32_t>(delta) == 0xf0000000) break;
                        sum += delta;
                        values[index] = static_cast<int16_t>(static_cast<uint16_t>(values[index]) + sum);
                    }
                    if (isBoneShort(descriptor))
                        positions[index] = std::ldexp(static_cast<float>(values[index]), shortShift) * mult_const;
                    else
                        positions[index] += std::ldexp(static_cast<float>(values[index]), floatShift) * floatOffsets[floatBone];
                    cursor += length;
                }
                if (!isBoneShort(descriptor)) ++floatBone;
            }
        }
        for (size_t i = 0; i < layout.bones * 3; ++i)
            if (!std::isfinite(positions[i])) return 0;
        std::copy_n(positions.begin(), layout.bones * 3, output.begin());
        return layout.bones;
    } catch (const std::exception&) {
        return 0;
    }
}
}
