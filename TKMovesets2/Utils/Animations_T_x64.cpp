#include <stdint.h>
#include <cstring>

// ============================= //

typedef unsigned char Byte;

// ============================= //

# define READ(type, addr) (*(type*)(addr))

struct s_weirdStack {
	const Byte* anim_ptr;
	int remainder;
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
			auto value = *weirdStack->anim_ptr;
			remainder = value | (remainder << 8);

			++weirdStack->anim_ptr;
		}
	}

	int v7 = starts_at_two - a2;
	weirdStack->starts_at_two = v7;
	weirdStack->remainder = remainder & ~(0xFFFFFFFF << v7);

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
			throw;
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


namespace TAnimUtils
{
	namespace FromMemory
	{
		uint16_t ParseAnimation0x64(const Byte* anim, unsigned int frame, float* pos_out)
		{
			uint64_t boneCount = READ(uint16_t, anim + 2);

			float pos_buffer[1552]{ 0 };

			float float_bones_offsets[256];
			int16_t v81_buf[768];

			if (frame == 0) {
				// Frame should be 1 for 1st frame
				throw;
				return 0;
			}

			if (boneCount >= 0x100) {
				throw;
				return 0;
			}

			// -- Get important informations about the animation (bone amount, length, amount of float bones, useful pointers) -- //

			const uint16_t* bone_descriptors = (uint16_t*)(anim + 4);

			uint64_t postBoneDescriptor_offset = (4 + boneCount * sizeof(uint16_t));
			const Byte* anim_postBoneDescriptorAddr = (Byte*)(anim + postBoneDescriptor_offset);

			uint64_t animLength = READ(uint16_t, anim_postBoneDescriptorAddr);
			uint64_t float_final_value_shift = READ(Byte, anim_postBoneDescriptorAddr + 3);
			uint64_t vv17 = READ(Byte, anim_postBoneDescriptorAddr + 2);

			uint64_t float_bones_amount = READ(uint16_t, anim_postBoneDescriptorAddr + 4); // Amount of bones that are in float format

			for (unsigned int i = 0; i < float_bones_amount; ++i) {
				// Offsets to apply to each float bone (one value = applied to all three axes)
				float_bones_offsets[i] = READ(float, anim_postBoneDescriptorAddr + 6 + i * sizeof(float));
			}

			int short_value_shift = vv17 & 0x7F; // Short values are shifted by this amount both in base pos & actual animation data

			if (frame > animLength) {
				frame = (unsigned int)animLength;
			}

			uint64_t base_pos_offset = 2 * ((4 * float_bones_amount + 6) / 2); // Offset of the base position, relative to the animation post-bone descriptor
			uint64_t base_pose_size = (boneCount * sizeof(uint16_t) * 3) + (float_bones_amount * sizeof(uint16_t) * 3);

			// -- Parse the base pos which is a bunch of short/floats, filling some other buffers in the process with the base pos data -- //
			{
				const Byte* __anim_ptr = &anim_postBoneDescriptorAddr[base_pos_offset];

				// Fill output buffer with base pos
				float* base_pos = pos_buffer;
				for (unsigned int i = 0; i < boneCount; ++i) {
					__anim_ptr += StoreBaseBoneValue(bone_descriptors[i], __anim_ptr, &base_pos[i * 3], &v81_buf[i * 3], short_value_shift);
					memcpy(&pos_out[i * 3], &base_pos[i * 3], sizeof(float) * 3);
				}
			}

			// -- Parse the frame we are looking for and add it to the base pos -- //

			// This is the post base-pose ptr
			const Byte* animPtr = &anim_postBoneDescriptorAddr[base_pos_offset + base_pose_size];

			// Calculate the keyframe to read from and its offset (-1 because input frame starts at 1 and not 0)
			unsigned int baseFrame = frame - 1;

			if (baseFrame == 0) {
				// Base pos is sufficient, no extra animation data to apply
				return (uint16_t)boneCount;
			}

			unsigned int targetSubKeyframe = baseFrame & 0xF; // Remove any bits at 16 and over : isolate the sub-keyframe number (0 - 16 - 32 - 48 - 64...)
			int keyframe = baseFrame / 16;
			unsigned int keyframe_offset = *(unsigned int*)(animPtr + 4 * keyframe);
			const Byte* current_value_ptr = animPtr + keyframe_offset;


			const uint16_t* bone_descriptors_ptr = bone_descriptors;
			const float* float_bones_offsets_ptr = float_bones_offsets;
			float* base_pos = pos_buffer;

			s_weirdStack weirdStack;
			uint16_t a2a_short;
			int __x1 = 0;
			int __x2 = 0;

			int16_t* v81_buf_ptr = v81_buf;

			// Loop on each bone
			for (unsigned int j = 0; j < boneCount; ++j)
			{
				// Loop on eaxh axis. XYZ, probably not in that order though iirc 
				for (int i = 0; i < 3; ++i)
				{
					Byte currentAnimValue = *current_value_ptr;

					int next_value_offset = currentAnimValue >> 2; // Offset to add to get to the next byte to read
					int offset_remainder = currentAnimValue & 3;

					weirdStack.anim_ptr = current_value_ptr + 1;
					weirdStack.remainder = offset_remainder;
					weirdStack.starts_at_two = 2;

					a2a_short = 0;
					*v81_buf_ptr += TK__Anim_64_sub_140733340(&weirdStack, &a2a_short, &__x1, &__x2);

					if (targetSubKeyframe)
					{
						// If we're in the middle of two keyframes, fetch the required data on what are seemingly the next frames to properly created the desired in-between
						int sum = 0;
						for (unsigned int subkeyframe = 0; subkeyframe < targetSubKeyframe; ++subkeyframe)
						{
							int result = TK__Anim_64_sub_140733340(&weirdStack, &a2a_short, &__x1, &__x2);
							if (result == 0xF0000000) { // End value
								break;
							}
							sum += result;
							*v81_buf_ptr += sum;
						}
					}

					int value = *v81_buf_ptr;

					if (isBoneShort(*bone_descriptors_ptr)) {
						// value was shifted right by [short_value_shift] in StoreBaseBoneValue, is shifted back here
						*pos_out = (float)(value << short_value_shift) * mult_const;
					}
					else {
						// Floats are apparently the only ones to build directly upon the base pos? I'm not sure how that works.
						*pos_out = (float)((float)(value << float_final_value_shift) * *float_bones_offsets_ptr) + *base_pos;
					}

					++pos_out;
					++base_pos;
					++v81_buf_ptr;
					current_value_ptr += next_value_offset;
				}

				if (!isBoneShort(*bone_descriptors_ptr)) {
					// Increment the float offset (every float bone has an offset that it applies to all three axes)
					float_bones_offsets_ptr++;
				}

				++bone_descriptors_ptr;
			}

			return (uint16_t)boneCount;
		}

	}
};