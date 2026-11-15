#pragma once

#include "MovesetStructs.h"
#include <algorithm>
#include <iterator>

// Legacy converters read the table and MOTA list as part of the fixed info prefix.
// Record offsets are relative to the structure block, independently of the file header.
template<class Blocks, class Info, size_t N>
bool ValidateLegacyLayout(const TKMovesetHeader& header, const Byte* payload, uint64_t size,
	const size_t (&strides)[N])
{
	if (header.block_list_size < sizeof(Blocks) / sizeof(uint64_t) || size < sizeof(Info)) return false;
	const auto& blocks = *reinterpret_cast<const Blocks*>(reinterpret_cast<const Byte*>(&header) + header.block_list);
	if (blocks.blocks[0] != 0 || !std::is_sorted(std::begin(blocks.blocks), std::end(blocks.blocks)) ||
		blocks.blocks[std::size(blocks.blocks) - 1] > size || blocks.nameBlock < sizeof(Info)) return false;
	const auto& info = *reinterpret_cast<const Info*>(payload);
	static_assert(N == std::extent_v<decltype(info.table.entries)>);
	auto available = blocks.animationBlock - blocks.movesetBlock;
	uint64_t remaining = available;
	for (size_t i = 0; i < N; ++i) {
		const auto& entry = info.table.entries[i];
		if (!strides[i] || !entry.listCount) continue;
		if (entry.listAddr > available || entry.listCount > (available - entry.listAddr) / strides[i] ||
			entry.listCount > remaining / strides[i]) return false;
		remaining -= entry.listCount * strides[i];
	}
	return true;
}
