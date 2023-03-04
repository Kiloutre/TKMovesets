#pragma once

#include <string>

#include "Importer.hpp"

#include "constants.h"
#include "Structs_t7.h"
#include "MovesetStructs.h"

using namespace StructsT7;

class DLLCONTENT ImporterT7 : public Importer
{
private:
	// In our locally allocated moveset, correct the mota lists offsets or copy the motas from the currently loaded character if we can't provide them
	void CorrectMotaList(MovesetHeader& header, char* movesetData, gameAddr gameMoveset, gameAddr playerAddress);
	// In our locally allocated movest, correct the lists pointing to the various moveset structure lists
	void CorrectPtrList(MovesetHeader& header, char* movesetData, gameAddr gameMoveset);
	

public:
	using Importer::Importer; // Inherit constructor too
	ImportationErrcode Import(const char* filename, gameAddr playerAddress, bool applyInstantly, float* progress) override;
	bool CanImport() override;

};
