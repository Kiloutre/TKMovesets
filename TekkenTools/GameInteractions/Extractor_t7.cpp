
#include "Extractor_t7.hpp"
#include "GameAddresses.h"
#include "GameAddressesFile.hpp"

// Todo: remove
#include <stdio.h>

void ExtractorT7::Extract(gameAddr playerAddress, float* progress)
{
	gameAddr movesetAddr = m_process->readInt64(playerAddress + GameAddressesFile::GetSingleValue("val_motbin_offset"));

	// Actual data starts at 0x2E0 but nothing in it interests us: we skip the first 8 bytes to go straight up to the character name
	gameAddr motbinAddr = movesetAddr + 0x2E0 + 0x8;
	gameAddr motbinEnd = m_process->readInt64(motbinAddr + 0x2D0);
	int32_t motbinSize = motbinEnd - motbinAddr;

	printf("motbin is %llx - %llx, size is %d (%dKB) (%dMB)\n", motbinAddr, motbinEnd, motbinSize, motbinSize / 1000, motbinSize / 1000 / 1000);
}