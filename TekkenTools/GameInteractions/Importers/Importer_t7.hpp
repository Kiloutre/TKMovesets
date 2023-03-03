#pragma once

#include <string>

#include "Importer.hpp"

#include "constants.h"
#include "Structs_t7.h"
#include "GameAddresses.h"

class DLLCONTENT ImporterT7 : public Importer
{
public:
	using Importer::Importer; // Inherit constructor too
	ImportationErrcode Import(const char* filename, gameAddr playerAddress, bool applyInstantly, float* progress) override;
	bool CanImport() override;

};
