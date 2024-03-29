#pragma once

#include "GameProcess.hpp"

#include "GameTypes.h"

struct s_animInfo
{
	unsigned int duration;
	uint32_t hash;
	uint64_t size;
};

namespace TAnimUtils
{
	// Get the size of the prefix to apply to T5 anims
	unsigned int GetT5AnimPrefixSize();
	// Write the preifx to a T5 anim in the proper endianness
	void GetT5AnimPrefix(Byte* buffer, bool little_endian);

	namespace FromMemory
	{
		// Byteswaps a MOTA block and its animations (little endian <-> big endian). Animations will be made to match the MOTA's endian.
		void ByteswapMota(Byte* motaAddr);
		// Byteswaps an animation (little endian <-> big endian)
		void ByteswapAnimation(Byte* animAddr);
		// Returns true if an animation is in little endian
		inline bool IsLittleEndian(const Byte* animAddr);
		
		// Returns an animation's duration in frames (60 per sec)
		uint32_t GetAnimDuration(const Byte* anim);
		// Returns a 0xC8 animation's duration in frames (60 per sec)
		uint32_t GetC8AnimDuration(const Byte* anim);
		// Returns a 0x64 animation's duration in frames (60 per sec)
		uint32_t Get64AnimDuration(const Byte* anim);

		// Returns the size in bytes of an animation
		uint64_t GetAnimSize(const Byte* anim);
		// Returns the size in bytes of a 0xC8 animation
		uint64_t getC8AnimSize(const Byte* anim);
		// Returns the size in bytes of a 0x64 animation
		uint64_t get64AnimSize(const Byte* anim);

		// Safe animation-size fetching functions that require providing the size of the file / memory area
		// If the animation data makes the size go above max_size (incomplete animation data), returns 0.
		namespace Safe
		{
			// Returns the size in bytes of an animation.
			uint64_t GetAnimSize(const Byte* anim, uint64_t max_size);
			// Returns the size in bytes of a 0xC8 animation.
			uint64_t getC8AnimSize(const Byte* anim, uint64_t max_size);
			// Returns the size in bytes of a 0x64 animation.
			uint64_t get64AnimSize(const Byte* anim, uint64_t max_size);
		};

		// Parse a specific frame of animation data, filling the float buffer with the appropriate values
		// Returns the amount of bones the animation contains which you may use to read the pos_out buffer (bone_count * 3 * float)
		uint16_t ParseAnimation0x64(const Byte* anim, unsigned int frame, float* pos_out);
	};

	namespace FromProcess
	{
		// Returns the size in bytes of a 0xC8 animation 
		uint64_t getC8AnimSize(const GameProcess& process, gameAddr anim);
		// Wrapper that calculates the size in bytes of a 0x64 animation (big or little endian)
		uint64_t get64AnimSize(const GameProcess& process, gameAddr anim);
		// Returns the size in bytes of a 0x64 animation (little endian anim)
		uint64_t get64AnimSize_LittleEndian(const GameProcess& process, gameAddr anim);
		// Returns the size in bytes of a 0x64 animation (big endian anim)
		uint64_t get64AnimSize_BigEndian(const GameProcess& process, gameAddr anim);

		// T5 0x64 anims lack anim type, bone count and bone descriptors.
		// They all are 0x64, 0x17 bones with the same bone descriptors
		uint64_t getT5_64AnimSize_LittleEndian(const GameProcess& process, gameAddr anim);
		uint64_t getT5_64AnimSize_BigEndian(const GameProcess& process, gameAddr anim);
	};

	namespace FromFile
	{
		// Gets animation duration in frame, size in bytes and crc32 hash and feeds it to a structure. Returns false on failure.
		bool GetAnimationInfos(const wchar_t* filename, s_animInfo& animInfo);

		// Gets the animation duration in frame or -1 if the file is invalid (validation isn't extensive)
		int GetAnimationDuration(const wchar_t* filename);
	};
};