#pragma once

#include <string>

#include "Importer.hpp"
#include "GameData.hpp"
#include "GameProcess.hpp"
#include "GameAddressesFile.hpp"

#include "constants.h"
#include "Structs_t7.h"
#include "GameAddresses.h"

class DLLCONTENT ImporterT7 : public Importer
{
public:
	using Importer::Importer; // Inherit constructor too
	void Import(const char* filename, gameAddr playerAddress, float* progress) override;
	bool CanImport() override;

};
