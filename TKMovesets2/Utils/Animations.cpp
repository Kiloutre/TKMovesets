#include <fstream>

#include "Animations.hpp"
#include "helpers.hpp"

#include "constants.h"

# define READ(addr, type) *(type*)(addr)

using namespace ByteswapHelpers;

namespace TAnimUtils
{
	char t5_anim_prefix[] = { 0x64, 0x00, 0x17, 0x00, 0x0B, 0x00, 0x0B, 0x00, 0x05, 0x00, 0x07, 0x00, 0x07, 0x00, 0x07, 0x00, 0x0B, 0x00, 0x07, 0x00, 0x07, 0x00, 0x07, 0x00, 0x07, 0x00, 0x06, 0x00, 0x07, 0x00, 0x07, 0x00, 0x07, 0x00, 0x06, 0x00, 0x07, 0x00, 0x07, 0x00, 0x06, 0x00, 0x07, 0x00, 0x07, 0x00, 0x06, 0x00, 0x07, 0x00 };
	char t5_anim_prefix_big_endian[] = { 0x00, 0x64, 0x00, 0x17, 0x00, 0x0B, 0x00, 0x0B, 0x00, 0x05, 0x00, 0x07, 0x00, 0x07, 0x00, 0x07, 0x00, 0x0B, 0x00, 0x07, 0x00, 0x07, 0x00, 0x07, 0x00, 0x07, 0x00, 0x06, 0x00, 0x07, 0x00, 0x07, 0x00, 0x07, 0x00, 0x06, 0x00, 0x07, 0x00, 0x07, 0x00, 0x06, 0x00, 0x07, 0x00, 0x07, 0x00, 0x06, 0x00, 0x07 };

	void GetT5AnimPrefix(Byte* buffer, bool little_endian)
	{
		if (little_endian) {
			memcpy(buffer, t5_anim_prefix, sizeof(t5_anim_prefix));
		}
		else {
			memcpy(buffer, t5_anim_prefix_big_endian, sizeof(t5_anim_prefix_big_endian));
		}
	}

	unsigned int GetT5AnimPrefixSize()
	{
		return sizeof t5_anim_prefix;
	}

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

			SWAP_INT16(motaAddr + 4); // Swap is_little_endian & is_big_endian values
			SWAP_INT16(motaAddr + 6);

			SWAP_INT32(motaAddr + 8);
			SWAP_INT32(motaAddr + 0xC); // Swap anim count

			for (unsigned int i = 0; i < animCount; ++i)
			{
				uint32_t listOffset = 0x14 + (i * sizeof(int));
				uint32_t animOffset = DEREF_UINT32(motaAddr + listOffset);

				SWAP_INT32(motaAddr + listOffset);

				// Make sure the animation matches the MOTA's endian
				uint32_t realAnimOffset = sourceIsBigEndian ? BYTESWAP_INT32(animOffset) : animOffset;

				bool animIsLittleEndian = *(motaAddr + realAnimOffset) != 0;
				bool animIsBigEndian = !animIsLittleEndian;
				if ((animIsBigEndian && sourceIsBigEndian) || (animIsLittleEndian && targetIsBigEndian)) {
					ByteswapAnimation(motaAddr + realAnimOffset);
				}
			}

			// This is the true byte used to determine if the animation is byteswaped or not
			Byte* mota0x11 = motaAddr + 0x11;
			*mota0x11 = *mota0x11 == 0;
		}


		void Byteswap64Animation(Byte* animAddr)
		{
			// Stack indicating whether a specific bone is 3x short or 3x floats
			// Could get rid of it and directly read the bone descriptors when needed
			bool isBoneInt16[256];

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

			SWAP_INT16(animAddr); // Byteswap anim type
			SWAP_INT16(animAddr + 2); // Byteswap bone count

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
				isBoneInt16[i] = 4 <= descriptor && descriptor <= 7;

				SWAP_INT16(animAddr + descriptorOffset);
			}

			uint32_t offset = 4 + boneCount * sizeof(uint16_t);

			uint16_t iterCount = DEREF_UINT16(animAddr + offset + 4);
			if (sourceIsBigEndian) {
				iterCount = BYTESWAP_INT16(iterCount);
			}

			SWAP_INT16(animAddr + offset); // Swap anim length
			//SWAP_INT16(animAddr + boneCount + 2); // This one isn't actually done, not in my old working byteswap scripts and not in T7's code either.
			SWAP_INT16(animAddr + offset + 4); // Swap iter count

			offset += 6;
			for (int i = 0; i < iterCount; ++i) {
				SWAP_INT32(animAddr + offset);
				offset += sizeof(int);
			}

			// Swap base position
			for (unsigned int i = 0; i < boneCount; ++i)
			{
				if (isBoneInt16[i])
				{
					SWAP_INT16(animAddr + offset);
					SWAP_INT16(animAddr + offset + 2);
					SWAP_INT16(animAddr + offset + 4);
					offset += sizeof(int16_t) * 3;
				}
				else
				{
					SWAP_INT32(animAddr + offset);
					SWAP_INT32(animAddr + offset + 4);
					SWAP_INT32(animAddr + offset + 8);
					offset += sizeof(float) * 3;
				}
			}

			// Byteswap keyframe informations
			unsigned int keyframeCount = (animLength + 14) >> 4;
			for (unsigned int i = 0; i < keyframeCount; ++i)
			{
				SWAP_INT32(animAddr + offset);
				offset += 4;
			}
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

			SWAP_INT16(animAddr + 0); // Swap anim type
			SWAP_INT16(animAddr + 2); // Swap bone count

			SWAP_INT32(animAddr + 4); // Swap anim length

			// Swap bone descriptor list
			for (unsigned int i = 0; i < boneCount; ++i)
			{
				int descriptorOffset = 8 + (i * sizeof(int));
				SWAP_INT32(animAddr + descriptorOffset);
			}

			// Swap every bone value (float)
			unsigned int headerSize = 8 + boneCount * sizeof(int);
			float* bonePtr = (float*)(animAddr + headerSize);

			for (unsigned int frame = 0; frame < animLength; ++frame)
			{
				for (unsigned int i = 0; i < boneCount; ++i)
				{
					SWAP_INT32(bonePtr);
					SWAP_INT32(bonePtr + 1);
					SWAP_INT32(bonePtr + 2);
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
				DEBUG_ERR("ByteswapAnimation() ERROR: INVALID ANIMATION TYPE '%x' FOR ADDR '%p'", animType, animAddr);
				throw;
				break;
			}
		}

		inline bool IsLittleEndian(const Byte* animAddr)
		{
			return animAddr[1] == 0;
		}


		uint32_t GetAnimDuration(const Byte* anim)
		{
			Byte animType = *anim;
			if (animType == 0) animType = *(anim + 1);

			switch (animType)
			{
			case 0x64:
				return GetC8AnimDuration(anim);
			case 0xC8:
				return Get64AnimDuration(anim);
			}

			DEBUG_ERR("Bad animation type: First four bytes 0x%X", READ(anim, uint32_t));
			return 0;
		}


		uint32_t GetC8AnimDuration(const Byte* anim)
		{
			bool isSwapped = !IsLittleEndian(anim);

			uint8_t bone_count = *(anim + (isSwapped ? 3 : 2));

			uint32_t length = READ(anim + 4, uint32_t);
			if (isSwapped) {
				length = BYTESWAP_INT32(length);
			}

			return length;
		}


		uint32_t Get64AnimDuration(const Byte* anim)
		{
			bool isSwapped = !IsLittleEndian(anim);

			uint64_t boneCount = READ(anim + 2, uint16_t);
			if (isSwapped) {
				boneCount = BYTESWAP_INT16(boneCount);
			}

			uint64_t postBoneDescriptor_offset = (4 + boneCount * sizeof(uint16_t));
			const Byte* anim_postBoneDescriptorAddr = anim + postBoneDescriptor_offset;

			uint16_t animLength = READ(anim_postBoneDescriptorAddr, uint16_t);
			if (isSwapped) {
				animLength = BYTESWAP_INT16(animLength);
			}

			return animLength;
		}


		uint64_t GetAnimSize(const Byte* anim)
		{
			Byte animType = *anim;
			if (animType == 0) animType = *(anim + 1);

			switch (animType)
			{
			case 0x64:
				return get64AnimSize(anim);
			case 0xC8:
				return getC8AnimSize(anim);
			}

			DEBUG_ERR("Bad animation type: First four bytes 0x%X", READ(anim, uint32_t));
			return 0;
		}


		uint64_t getC8AnimSize(const Byte* anim)
		{
			bool isSwapped = !IsLittleEndian(anim);

			uint8_t bone_count = *(anim + (isSwapped ? 3 : 2));
			uint32_t header_size = bone_count * 0x4 + 0x8;
			uint32_t frame_size = bone_count * 0x4 * 3;

			uint32_t length = READ(anim + 4, uint32_t);
			if (isSwapped) {
				length = BYTESWAP_INT32(length);
			}

			return (int64_t)header_size + (int64_t)frame_size * (int64_t)length;
		}


		uint64_t get64AnimSize(const Byte* anim)
		{
			bool isSwapped = *anim == 0;
			// Do all calculations in uint64_t that way i don't have to pay attention to possible overflows

			uint64_t boneCount = READ(anim + 2, uint16_t);
			if (isSwapped) {
				boneCount = BYTESWAP_INT16(boneCount);
			}

			uint64_t postBoneDescriptor_offset = (4 + boneCount * sizeof(uint16_t));
			const Byte* anim_postBoneDescriptorAddr = anim + postBoneDescriptor_offset;

			uint64_t animLength = READ(anim_postBoneDescriptorAddr, uint16_t);
			uint64_t __unknown__ = READ(anim_postBoneDescriptorAddr + 4, uint16_t);
			if (isSwapped) {
				animLength = BYTESWAP_INT16(animLength);
				__unknown__ = BYTESWAP_INT16(__unknown__);
			}

			uint64_t vv73 = 2 * ((4 * __unknown__ + 6) / 2);
			uint64_t aa4 = 6 * (__unknown__ + boneCount);

			const Byte* animPtr = anim_postBoneDescriptorAddr + vv73 + aa4;

			unsigned int baseFrame = (unsigned int)animLength - (animLength >= 2 ? 2 : 1);
			unsigned int keyframe = baseFrame / 16;
			unsigned int _v56_intPtr = READ(animPtr + 4 * (uint64_t)keyframe, unsigned int);
			if (isSwapped) {
				_v56_intPtr = BYTESWAP_INT32(_v56_intPtr);
			}

			const Byte* animPtr_2 = animPtr + _v56_intPtr;
			int lastArg_copy = (int)boneCount;

			do
			{
				for (int i = 0; i < 3; ++i)
				{
					Byte v58 = *animPtr_2;
					int offsetStep = v58 / 4;
					animPtr_2 += offsetStep;
				}
			} while (--lastArg_copy != 0);

			return (uint64_t)animPtr_2 - (uint64_t)anim;
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

		uint64_t get64AnimSize_BigEndian(GameProcess* process, gameAddr anim)
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

		uint64_t get64AnimSize_LittleEndian(GameProcess* process, gameAddr anim)
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



		uint64_t getT5_64AnimSize_BigEndian(GameProcess* process, gameAddr anim)
		{
			// Do all calculations in uint64_t that way i don't have to pay attention to possible overflows

			uint64_t boneCount = 0x17;

			gameAddr anim_postBoneDescriptorAddr = anim;

			uint64_t animLength = process->readUInt16(anim_postBoneDescriptorAddr);
			uint64_t __unknown__ = process->readUInt16(anim_postBoneDescriptorAddr + 4);
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

		uint64_t getT5_64AnimSize_LittleEndian(GameProcess* process, gameAddr anim)
		{
			// Do all calculations in uint64_t that way i don't have to pay attention to possible overflows

			uint64_t boneCount = 0x17;

			gameAddr anim_postBoneDescriptorAddr = anim;

			uint64_t animLength = process->readUInt16(anim_postBoneDescriptorAddr);
			uint64_t __unknown__ = process->readUInt16(anim_postBoneDescriptorAddr + 4);

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

	namespace FromFile
	{
		int GetAnimationDuration(const wchar_t* filename)
		{
			std::ifstream file(filename, std::ios::binary);

			if (file.fail()) {
				return -1;
			}

			char buf[8];
			int duration = -1;

			file.read(buf, 8);
			if (file.gcount() == 8) {
				Byte isBigEndian = buf[0] == 0;
				Byte animType = isBigEndian ? buf[1] : buf[0];
				Byte boneCount = isBigEndian ? buf[3] : buf[2];

				switch (animType)
				{
				case 0x64:
					file.seekg((uint64_t)(4 + boneCount * 2), std::ios::beg);
					if (!file.fail() && !file.bad()) {
						file.read(buf, 2);
						if (file.gcount() == 2) {
							duration = *(uint16_t*)buf;
							if (isBigEndian) {
								duration = BYTESWAP_INT16(duration);
							}
						}

					}
					break;
				case 0xC8:
					
					duration = DEREF_INT32(buf + 4);
					if (isBigEndian) {
						duration = BYTESWAP_INT32(duration);
					}
					break;
				}
			}
            
			return duration;
		}
	};
};