#pragma once

#include <string>

#include "Extractor.hpp"

#include "constants.h"
#include "Structs_t5.h"
#include "MovesetStructs.h"

// This class has no storage and i would like to keep it that way.
// You should be passing values through method calls and no other way.
// It should be able to call Export() while another Export() is going on in another thread and this without breaking anything

class ExtractorT5 : public Extractor
{
private:
	/// Main functions

	// Fill the moveset table & its corresponding offset table brother.
	void FillMovesetTables(gameAddr movesetAddr, StructsT5::MovesetTable* table, StructsT5::MovesetTable* offsets);
	// Fill moveset header with our own various useful informations
	void FillHeaderInfos(TKMovesetHeader& infos, gameAddr playerAddress, uint64_t propertyCount);
	// Allocate and copy the contents of the moveset block
	Byte* CopyMovesetBlock(gameAddr movesetAddr, uint64_t& size_out, const StructsT5::MovesetTable& table, const StructsT5::MovesetTable* offsets);
	// Allocate and copy the contents of the name block
	char* CopyNameBlock(gameAddr movesetAddr, uint64_t& size_out, const StructsT5::Move* movelist, uint64_t moveCount, gameAddr& nameBlockStart);
	// Fill motalist struct, build list of anims within the mota file, allocate and copy contents of the mota block
	Byte* CopyMotaBlocks(gameAddr movesetAddr, uint64_t& size_out, StructsT5::MotaList* motasList, ExtractSettings settings);
	// Returns an allocated block containing animations that weren't in the main animation block. Also builds a map to convert anim addresses to offsets.
	Byte* CopyAnimations(const StructsT5::Move* movelist, size_t moveCount, uint64_t& size_out, std::map<gameAddr, uint64_t>& offsets);
	// Copies the moveset info block (aliases, couple strings)
	void CopyMovesetInfoBlock(gameAddr movesetAddr, StructsT5::MovesetInfo* movesetHeader);
	// Map containing the game character names to legible character names
    std::map<std::string, std::string> m_characterNamesMap = {
        { "\x95""\x97""\x8a""\xd4"" \x90""m", "Jin" },
        { "BAEK DOO SAN", "Baek Doo San" },
        { "[\x83""G\x83""f\x83""B\x81""E\x83""S\x83""\x8b""\x83""h\x81""[]", "Eddy" },
        { "[DEVIL JIN]", "Devil Jin" },
        { "[\x8d""\x95""\x90""l\x94""E\x8e""\xd2""]", "Raven" },
        { "[ \x94""\xf2""\x92""\xb9"" ]", "Asuka" },
        { "[\x91""\xbe""\x8b""\xc9""\x8c""\x9d""]", "Feng" },
        { "[\x8c""\xb5""\x97""\xb3""]", "Ganryu" },
        { "[ \x89""\xa4"" \x96""\xb8""\x97""\x8b"" ]", "Wang" },
        { "[\x83""A\x83""}\x83""L\x83""\x93""]", "Armor King" },
        { "[\x93""S\x8c""\x9d""5\x83""{\x83""X]", "Jinpachi" },
        { "[ VALE-TUDO ]", "Marduk" },
        { "[ANNA]", "Anna" },
        { "[ \x83""\x8d""\x83""E ]", "Law" },
        { "[\x89""\xd4""\x98""Y]", "Hwoarang" },
        { "[\x83""L\x83""\x93""\x83""O]", "King" },
        { "[EMILIE]", "Emilie" },
        { "[\x90""V\x83""L\x83""\x83""\x83""\x89""(\x91""\xe5""\x8d""\xb2"")]", "Dragunov" },
        { "\x8e""O\x93""\x87"" \x95""\xbd""\x94""\xaa", "Heihachi" },
        { "[\x83""N\x83""\x8a""\x83""X\x83""e\x83""B]", "Christie" },
        { "[\x83""|\x81""[\x83""\x8b""]", "Paul" },
        { "[\x83""W\x83""\x83""\x83""b\x83""N\x82""T]", "Jack-5" },
        { "\x83""u\x83""\x8b""\x81""[\x83""X", "Bruce" },
        { "[\x83""\x8d""\x83""W\x83""\x83""\x81""[]", "Roger Jr." },
        { "[ \x97""\x8b"" \x95""\x90""\x97""\xb4"" ]", "Lei Wulong" },
        { "[\x83""j\x81""[\x83""i]", "Nina" },
        { "\x83""{\x83""N\x83""T\x81""[", "Steve Fox" },
        { "[ \x8e""O\x93""\x87"" \x88""\xea""\x94""\xaa"" ]", "Kazuya" },
        { "\x97""\xbd"" \x8b""\xc5""\x89""J", "Xiaoyu" },
        { "[ \x97""\x9b"" \x92""\xb4""\x98""T ]", "Lee" },
        { "[ \x83""W\x83""\x85""\x83""\x8a""\x83""A ]", "Julia" },
        { "\x8b""g\x8c""\xf5", "Yoshimitsu" },
        { "\x83""u\x83""\x89""\x83""C\x83""A\x83""\x93""", "Bryan" },
        { "[\x83""N\x83""}]", "Panda" }
    };

	/// Helpers

	// Writes bounds of the block containing anim and move names
	void GetNamesBlockBounds(const StructsT5::Move* move, uint64_t moveCount, gameAddr& start, gameAddr& end);
	// Returns an allocated block containing mota block offsets followed by mota blocks
	Byte* AllocateMotaCustomBlock(StructsT5::MotaList* motas, uint64_t& size_out, ExtractSettings settings);
	// Calculate the size of the mota custom block we will build, and also fill boundaries with every animation address we can find
	uint64_t CalculateMotaCustomBlockSize(const StructsT5::MotaList* motas, std::map<gameAddr32, std::pair<uint32_t, uint32_t>>& offsetMap, ExtractSettings settings);

public:
	using Extractor::Extractor; // Inherit constructor too
	ExtractionErrcode_ Extract(gameAddr playerAddress, ExtractSettings settings, uint8_t& progress) override;
	bool CanExtract() override;

	std::string GetPlayerCharacterName(gameAddr playerAddress) override;
	uint32_t GetCharacterID(gameAddr playerAddress) override;
	gameAddr GetCharacterAddress(uint8_t playerId) override;
	std::vector<gameAddr> GetCharacterAddresses() override;
};
