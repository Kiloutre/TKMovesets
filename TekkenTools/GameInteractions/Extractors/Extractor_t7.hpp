#pragma once

#include "Extractor.hpp"
#include "GameData.hpp"
#include "GameProcess.hpp"
#include "GameAddresses.h"
#include "GameAddressesFile.hpp"
#include "Structs_t7.h"

class T7Moveset
{
public:
	// Filename to save in
	std::string filename;
	// Origin (Game name + author)
	std::string origin;
	// Target character to play on
	std::string target_character;
	// Date of last modification
	std::string date;

	// All the lists in the moveset
	t7structs::movesetLists* lists;
	// Block containing moveset infos and aliases
	void* movesetInfoBlock;
	// Block containing list data
	void* movesetBlock;
	// Block containing the animations
	void* animationBlock;
};

class ExtractorT7 : public Extractor
{
public:
	using Extractor::Extractor; // Inherit constructor too
	void Extract(gameAddr playerAddress, float* progress) override;
};
