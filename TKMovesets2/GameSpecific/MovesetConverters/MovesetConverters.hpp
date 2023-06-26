#pragma once

#include "constants.h"
#include "MovesetStructs.h"
#include "GameTypes.h"

#include "Structs_t7.h"

struct s_propAlias {
	unsigned int target_id = 0;
	std::map<unsigned int, unsigned int> param_alias;
	bool nofill = false;
};

namespace MovesetConverter
{
	// T7
	class TTT2ToT7
	{
	private:
		virtual std::map<unsigned int, s_propAlias> InitAliasDictionary();
	public:

		bool Convert(const TKMovesetHeader* header, Byte*& moveset, uint64_t& s_moveset, StructsT7::TKMovesetHeaderBlocks& blocks_out);
	};

	// TREV just has different aliases from TTT2 so inherit from it
	class TREVToT7 : public TTT2ToT7
	{
	private:
		std::map<unsigned int, s_propAlias> InitAliasDictionary() override;
	};


	class T6ToT7
	{
	private:
		virtual std::map<unsigned int, s_propAlias> InitAliasDictionary();
	public:
		bool Convert(const TKMovesetHeader* header, Byte*& moveset, uint64_t& s_moveset, StructsT7::TKMovesetHeaderBlocks& blocks_out);
	};


	class T5ToT7
	{
	protected:
		virtual std::map<unsigned int, s_propAlias> InitAliasDictionary();
	public:

		bool Convert(const TKMovesetHeader* header, Byte*& moveset, uint64_t& s_moveset, StructsT7::TKMovesetHeaderBlocks& blocks_out);
	};

	// T5 has both a slightly different structure and different aliases
	class T5DRToT7 
	{
	private:
		virtual std::map<unsigned int, s_propAlias> InitAliasDictionary();
	public:
		bool Convert(const TKMovesetHeader* header, Byte*& moveset, uint64_t& s_moveset, StructsT7::TKMovesetHeaderBlocks& blocks_out);
	};
};