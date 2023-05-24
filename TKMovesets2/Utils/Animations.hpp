#pragma once

#include "GameProcess.hpp"

#include "GameTypes.h"

namespace TAnimUtils
{
	namespace FromMemory
	{
		// Byteswaps a MOTA block and its animations (little endian <-> big endian). Animations will be made to match the MOTA's endian.
		void ByteswapMota(Byte* motaAddr);
		// Byteswaps an animation (little endian <-> big endian)
		void ByteswapAnimation(Byte* animAddr);
		// Returns true if an animation is in little endian
		bool IsLittleEndian(Byte* animAddr);
	};

	namespace FromProcess
	{
		// Returns the size in bytes of a 0xC8 animation 
		uint64_t getC8AnimSize(GameProcess* process, gameAddr anim);
		// Wrapper that calculates the size in bytes of a 0x64 animation (big or little endian)
		uint64_t get64AnimSize(GameProcess* process, gameAddr anim);
		// Returns the size in bytes of a 0x64 animation (little endian anim)
		uint64_t get64AnimSize_LittleEndian(GameProcess* process, gameAddr anim);
		// Returns the size in bytes of a 0x64 animation (big endian anim)
		uint64_t get64AnimSize_BigEndian(GameProcess* process, gameAddr anim);
	};

	namespace FromFile
	{
		// Gets the animation duration in frame or -1 if the file is invalid (validation isn't extensive)
		int GetAnimationDuration(const wchar_t* filename);
	};
};