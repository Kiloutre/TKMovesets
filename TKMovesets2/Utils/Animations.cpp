#include <fstream>

#include "Animations.hpp"
#include "helpers.hpp"

#include "constants.h"

using namespace ByteswapHelpers;

namespace TAnimUtils
{
	/*
	namespace FromMemory
	{
		void ByteswapMota(Byte* motaAddr)
		{
			bool sourceIsBigEndian = motaAddr[5] == 1;
			bool targetIsBigEndian = !sourceIsBigEndian;

			uint32_t animCount = DEREF_UINT32(motaAddr + 0xC);
			if (sourceIsBigEndian) {
				animCount = BYTESWAP_INT32(animCount);
			}

			SWAP_SHORT(motaAddr + 4); // Swap is_little_endian & is_big_endian values
			SWAP_SHORT(motaAddr + 6);

			SWAP_INT(motaAddr + 8);
			SWAP_INT(motaAddr + 0xC); // Swap anim count

			for (unsigned int i = 0; i < animCount; ++i)
			{
				uint32_t listOffset = 0x14 + (i * sizeof(int));
				uint32_t animOffset = DEREF_UINT32(motaAddr + listOffset);

				SWAP_INT(motaAddr + listOffset);

				// Make sure the animation matches the MOTA's endian
				uint32_t realAnimOffset = sourceIsBigEndian ? BYTESWAP_INT32(animOffset) : animOffset;

				bool animIsLittleEndian = *(motaAddr + realAnimOffset) != 0;
				bool animIsBigEndian = !animIsLittleEndian;
				if ((animIsBigEndian && sourceIsBigEndian) || (animIsLittleEndian && targetIsBigEndian)) {
					ByteswapAnimation(motaAddr + realAnimOffset);
				}
			}

			Byte* mota0x11 = motaAddr + 0x11;
			*mota0x11 = *mota0x11 == 0;
		}


		void Byteswap64Animation(Byte* animAddr)
		{
			bool isBoneFloat[256]; // Stack indicating whether a specific bone is 3x short or 3x floats

			bool sourceIsBigEndian = animAddr[0] == 0;
			bool targetIsBigEndian = !sourceIsBigEndian;

			// Get important informations
			uint16_t boneCount = DEREF_UINT16(animAddr + 2);
			if (sourceIsBigEndian) {
				boneCount = BYTESWAP_INT16(boneCount);
			}
			uint16_t animLength = DEREF_UINT16(animAddr + 4 + boneCount * 2);
			if (sourceIsBigEndian) {
				animLength = BYTESWAP_INT16(animLength);
			}

			SWAP_SHORT(animAddr); // Byteswap anim type
			SWAP_SHORT(animAddr + 2); // Byteswap bone count

			// Swap bone descriptor list and fill isBoneInt16;
			for (unsigned int i = 0; i < boneCount; ++i)
			{
				int descriptorOffset = 4 + (i * sizeof(uint16_t));
				uint16_t descriptor = DEREF_UINT16(animAddr + descriptorOffset);
				if (sourceIsBigEndian) {
					descriptor = BYTESWAP_INT16(descriptor);
				}

				// The overflow here is purposeful (casting the result of the substraction to unsigned)
				// This makes bone descriptor 0x3 a float, and anything over 0x7 (so 0xB) also a float
				isBoneFloat[i] = (uint16_t)(descriptor - 4) > 3;

				SWAP_SHORT(animAddr + descriptorOffset);
			}

			uint32_t offset = 4 + boneCount * sizeof(uint16_t);

			uint16_t iterCount = DEREF_UINT16(animAddr + offset + 4);
			if (sourceIsBigEndian) {
				iterCount = BYTESWAP_INT16(iterCount);
			}

			SWAP_SHORT(animAddr + offset); // Swap anim length
			//SWAP_SHORT(animAddr + boneCount + 2); // This one isn't actually done, not in my old working byteswap scripts and not in T7's code either.
			SWAP_SHORT(animAddr + offset + 4); // Swap iter count

			offset += 6;
			for (int i = 0; i < iterCount; ++i) {
				SWAP_INT(animAddr + offset);
				offset += sizeof(int);
			}

			// Swap base position
			for (unsigned int i = 0; i < boneCount; ++i)
			{
				if (isBoneFloat[i])
				{
					SWAP_INT(animAddr + offset);
					SWAP_INT(animAddr + offset + 4);
					SWAP_INT(animAddr + offset + 8);
					offset += sizeof(float) * 3;
				}
				else
				{
					SWAP_SHORT(animAddr + offset);
					SWAP_SHORT(animAddr + offset + 2);
					SWAP_SHORT(animAddr + offset + 4);
					offset += sizeof(int16_t) * 3;
				}
			}

			// Byteswap keyframe informations
			int keyframeCount = (animLength + 14) >> 4;

			do
			{
				SWAP_INT(animAddr + offset);
				offset += 4;
			} while (--keyframeCount);
		}

		void ByteswapC8Animation(Byte* animAddr)
		{
			bool sourceIsBigEndian = animAddr[0] == 0;
			bool targetIsBigEndian = !sourceIsBigEndian;

			uint16_t boneCount = DEREF_UINT16(animAddr + 2);
			uint32_t animLength = DEREF_UINT32(animAddr + 4);
			if (sourceIsBigEndian) {
				boneCount = BYTESWAP_INT16(boneCount);
				animLength = BYTESWAP_INT32(animLength);
			}

			SWAP_SHORT(animAddr + 0); // Swap anim type
			SWAP_SHORT(animAddr + 2); // Swap bone count

			SWAP_INT(animAddr + 4); // Swap anim length

			// Swap bone descriptor list
			for (unsigned int i = 0; i < boneCount; ++i)
			{
				int descriptorOffset = 8 + (i * sizeof(int));
				SWAP_INT(animAddr + descriptorOffset);
			}

			// Swap every bone value (float)
			unsigned int headerSize = 8 + boneCount * sizeof(int);
			float* bonePtr = (float*)(animAddr + headerSize);

			for (unsigned int frame = 0; frame < animLength; ++frame)
			{
				for (unsigned int i = 0; i < boneCount; ++i)
				{
					SWAP_INT(bonePtr);
					SWAP_INT(bonePtr + 1);
					SWAP_INT(bonePtr + 2);
					bonePtr += 3;
				}
			}
		}

		void ByteswapAnimation(Byte* animAddr)
		{
			Byte animType = animAddr[0] == 0 ? animAddr[1] : animAddr[0];
			switch (animType)
			{
			case 0x64:
				Byteswap64Animation(animAddr);
				break;
			case 0xC8:
				ByteswapC8Animation(animAddr);
				break;
			default:
				DEBUG_LOG("!! ByteswapAnimation() ERROR: INVALID ANIMATION TYPE '%x' FOR ADDR '%p' !!\n", animType, animAddr);
				throw;
				break;
			}
		}

	};

	namespace FromProcess
	{
		uint64_t getC8AnimSize(GameProcess* process, gameAddr anim)
		{
			bool isSwapped = process->readInt8(anim + 0) == 0;

			uint8_t bone_count = process->readInt8(anim + (isSwapped ? 3 : 2));
			uint32_t header_size = bone_count * 0x4 + 0x8;
			uint32_t frame_size = bone_count * 0x4 * 3;

			uint32_t length = process->readInt32(anim + 4);
			if (isSwapped) {
				length = BYTESWAP_INT32(length);
			}

			return (int64_t)header_size + (int64_t)frame_size * (int64_t)length;
		}

		uint64_t get64AnimSize(GameProcess* process, gameAddr anim)
		{
			bool isSwapped = process->readInt8(anim + 0) == 0;
			return isSwapped ? get64AnimSize_BigEndian(process, anim) : get64AnimSize_LittleEndian(process, anim);
		}

		uint64_t get64AnimSize_LittleEndian(GameProcess* process, gameAddr anim)
		{
			// Do all calculations in uint64_t that way i don't have to pay attention to possible overflows

			uint64_t boneCount = process->readInt16(anim + 2);
			boneCount = BYTESWAP_INT16(boneCount);

			uint64_t postBoneDescriptor_offset = (4 + boneCount * sizeof(uint16_t));
			gameAddr anim_postBoneDescriptorAddr = (gameAddr)(anim + postBoneDescriptor_offset);

			uint64_t animLength = (uint16_t)process->readInt16(anim_postBoneDescriptorAddr);
			uint64_t __unknown__ = (uint16_t)process->readInt16(anim_postBoneDescriptorAddr + 4);
			animLength = BYTESWAP_INT16(animLength);
			__unknown__ = BYTESWAP_INT16(__unknown__);

			uint64_t vv73 = 2 * ((4 * __unknown__ + 6) / 2);
			uint64_t aa4 = 6 * (__unknown__ + boneCount);

			gameAddr animPtr = anim_postBoneDescriptorAddr + vv73 + aa4;

			unsigned int baseFrame = (unsigned int)animLength - (animLength >= 2 ? 2 : 1);
			unsigned int keyframe = baseFrame / 16;
			unsigned int _v56_intPtr = (unsigned int)process->readInt32(animPtr + 4 * (uint64_t)keyframe);
			_v56_intPtr = BYTESWAP_INT32(_v56_intPtr);

			gameAddr animPtr_2 = animPtr + _v56_intPtr;
			int lastArg_copy = (int)boneCount;

			do
			{
				for (int i = 0; i < 3; ++i)
				{
					Byte v58 = process->readInt8(animPtr_2);
					int offsetStep = v58 / 4;
					animPtr_2 += offsetStep;
				}
			} while (--lastArg_copy != 0);

			return (uint64_t)animPtr_2 - (uint64_t)anim;
		}

		uint64_t get64AnimSize_BigEndian(GameProcess* process, gameAddr anim)
		{
			// Do all calculations in uint64_t that way i don't have to pay attention to possible overflows

			uint64_t boneCount = process->readInt16(anim + 2);

			uint64_t postBoneDescriptor_offset = (4 + boneCount * sizeof(uint16_t));
			gameAddr anim_postBoneDescriptorAddr = (gameAddr)(anim + postBoneDescriptor_offset);

			uint64_t animLength = (uint16_t)process->readInt16(anim_postBoneDescriptorAddr);
			uint64_t __unknown__ = (uint16_t)process->readInt16(anim_postBoneDescriptorAddr + 4);

			uint64_t vv73 = 2 * ((4 * __unknown__ + 6) / 2);
			uint64_t aa4 = 6 * (__unknown__ + boneCount);

			gameAddr animPtr = anim_postBoneDescriptorAddr + vv73 + aa4;

			unsigned int baseFrame = (unsigned int)animLength - (animLength >= 2 ? 2 : 1);
			unsigned int keyframe = baseFrame / 16;
			unsigned int _v56_intPtr = (unsigned int)process->readInt32(animPtr + 4 * (uint64_t)keyframe);

			gameAddr animPtr_2 = animPtr + _v56_intPtr;
			int lastArg_copy = (int)boneCount;

			do
			{
				for (int i = 0; i < 3; ++i)
				{
					Byte v58 = process->readInt8(animPtr_2);
					int offsetStep = v58 / 4;
					animPtr_2 += offsetStep;
				}
			} while (--lastArg_copy != 0);

			return (uint64_t)animPtr_2 - (uint64_t)anim;
		}

	};
	*/

	namespace FromFile
	{
		int GetAnimationDuration(const wchar_t* filename)
		{
			std::ifstream file(filename, std::ios::binary);

			if (file.fail()) {
				return -1;
			}

			char* buf = new char[8];
			int duration = -1;

			file.read(buf, 8);
			if (file.gcount() == 8) {
				Byte animType = buf[0];
				Byte boneCount = buf[2];

				switch (animType)
				{
				case 0xC8:
					duration = *(uint16_t*)&buf[4];
					break;
				case 0x64:
					file.seekg(4 + (int)boneCount * 2, std::ios::beg);
					if (!file.fail() && !file.bad()) {
						file.read(buf, 2);
						if (file.gcount() == 2) {
							duration = *(uint16_t*)buf;
						}

					}
					break;
				}
			}
			delete[] buf;
			return duration;
		}
	};
};