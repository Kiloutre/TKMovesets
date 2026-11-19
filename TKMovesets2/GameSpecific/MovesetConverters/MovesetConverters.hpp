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
		// TTT2 rules voted against T7 movesets (Converter_ttt2_to_t7_Votes.inc)
		virtual void ApplyVotedRules(std::map<unsigned int, s_propAlias>& aliases);
		virtual void ConvertHitbox(unsigned int& hitbox);
		// TTT2's tag button, bit 0x100 of a cancel's button word (tag changes, tag throws, tag-in moves)
		static constexpr uint32_t TagButton = 0x100;
		virtual bool HasTagButton() const { return true; }
	public:

		bool Convert(const TKMovesetHeader* header, Byte*& moveset, uint64_t& s_moveset, StructsT7::TKMovesetHeaderBlocks& blocks_out);
	};

	// TREV just has different aliases from TTT2 so inherit from it
	class TREVToT7 : public TTT2ToT7
	{
	private:
		std::map<unsigned int, s_propAlias> InitAliasDictionary() override;
		void ApplyVotedRules(std::map<unsigned int, s_propAlias>&) override {} // the votes are TTT2's
		void ConvertHitbox(unsigned int& hitbox) override;
		bool HasTagButton() const override { return false; }
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