#include <algorithm>
#include <set>
#include <vector>
#include "AnimationReader.hpp"
#include "Helpers.hpp"
#include "Importer_t7_Utils.hpp"

// Turns an index into an absolute address, or NULL (0) if index is -1
template<class T>
static bool RelocateIndex(gameAddr& index, gameAddr base, gameAddr listOffset, uint64_t count)
{
	if (index == UINT64_MAX) {
		index = 0;
		return true;
	}
	if (index >= count) return false;
	index = base + listOffset + index * sizeof(T);
	return true;
}


namespace T7ImportUtils
{
	bool ValidateT7Layout(const StructsT7::TKMovesetHeaderBlocks& offsets, const Byte* payload, uint64_t size)
	{
		using namespace StructsT7;
		if (!payload || size < sizeof(StructsT7::MovesetInfo) || offsets.movesetInfoBlock != 0 || offsets.movelistBlock > size ||
			!std::is_sorted(std::begin(offsets.blocks), std::end(offsets.blocks))) return false;
		if (offsets.tableBlock != offsetof(MovesetInfo, table) ||
			offsets.motalistsBlock != offsetof(MovesetInfo, motas) ||
			offsets.nameBlock - offsets.motalistsBlock < sizeof(MotaList)) return false;

		const auto& table = *reinterpret_cast<const StructsT7_gameAddr::MovesetTable*>(payload + offsets.tableBlock);
		const size_t strides[] = {
			sizeof(Reactions), sizeof(Requirement), sizeof(HitCondition), sizeof(Projectile),
			sizeof(Pushback), sizeof(PushbackExtradata), sizeof(Cancel), sizeof(Cancel),
			sizeof(CancelExtradata), sizeof(ExtraMoveProperty), sizeof(OtherMoveProperty),
			sizeof(OtherMoveProperty), sizeof(Move), sizeof(Voiceclip), sizeof(InputSequence),
			sizeof(Input), sizeof(UnknownParryRelated), sizeof(CameraData), sizeof(ThrowCamera)
		};
		static_assert(std::size(strides) == std::extent_v<decltype(table.entries)>);
		uint64_t structuresSize = offsets.animationBlock - offsets.movesetBlock;
		for (size_t i = 0; i < std::size(strides); ++i) {
			const auto& entry = table.entries[i];
			if (entry.listAddr > structuresSize || entry.listCount > (structuresSize - entry.listAddr) / strides[i]) return false;
		}
        std::vector<std::pair<uint64_t, uint64_t>> occupied;
        for (size_t i = 0; i < std::size(strides); ++i) {
            const auto& entry = table.entries[i];
            if (entry.listCount) occupied.emplace_back(entry.listAddr, entry.listAddr + entry.listCount * strides[i]);
        }
        std::sort(occupied.begin(), occupied.end());
        for (size_t i = 1; i < occupied.size(); ++i)
            if (occupied[i].first < occupied[i - 1].second) return false;
		const auto& motas = *reinterpret_cast<const StructsT7_gameAddr::MotaList*>(payload + offsets.motalistsBlock);
		const auto motaSize = offsets.movelistBlock - offsets.motaBlock;
		for (auto offset : motas.motas) {
			if (offset != MOVESET_ADDR_MISSING && (offset > motaSize || motaSize - offset < 4)) return false;
		}
		auto moves = reinterpret_cast<const StructsT7_gameAddr::Move*>(payload + offsets.movesetBlock + table.move);
		const auto namesSize = offsets.movesetBlock - offsets.nameBlock;
		const Byte* names = payload + offsets.nameBlock;
		for (size_t i = 0; i < table.moveCount; ++i) {
			const auto& move = moves[i];
			if (move.anim_addr >= offsets.motaBlock - offsets.animationBlock) return false;
			for (auto offset : {move.name_addr, move.anim_name_addr}) {
				if (offset >= namesSize || !memchr(names + offset, 0, namesSize - offset)) return false;
			}
		}
		const auto listSize = size - offsets.movelistBlock;
        if (listSize > 0 && listSize < 4 && !memcmp(payload + offsets.movelistBlock, "MVLT", listSize)) return false;
		if (listSize >= 4 && memcmp(payload + offsets.movelistBlock, "MVLT", 4) == 0) {
			if (listSize < offsetof(MvlHead, translation_strings)) return false;
			auto& list = *reinterpret_cast<const MvlHead*>(payload + offsets.movelistBlock);
			if (list.displayables_offset > listSize || list.playables_offset > listSize || list.inputs_offset > listSize ||
				list.displayables_count < 0 || list.playables_count < 0 ||
				list.displayables_count > (listSize - list.displayables_offset) / sizeof(MvlDisplayable) ||
				list.playables_count > (listSize - list.playables_offset) / sizeof(MvlPlayable)) return false;
            const auto* displayables = reinterpret_cast<const MvlDisplayable*>(payload + offsets.movelistBlock + list.displayables_offset);
            for (int i = 0; i < list.displayables_count; ++i) {
                for (auto offset : displayables[i].all_translation_offsets)
                    if (offset < 0 || static_cast<uint64_t>(offset) >= listSize ||
                        !memchr(payload + offsets.movelistBlock + offset, 0, listSize - offset)) return false;
            }
			auto* entries = reinterpret_cast<const MvlPlayable*>(payload + offsets.movelistBlock + list.playables_offset);
			auto inputCount = (listSize - list.inputs_offset) / sizeof(MvlInput);
			for (int i = 0; i < list.playables_count; ++i) {
				if (entries[i].input_sequence_offset > inputCount ||
					entries[i].input_count > inputCount - entries[i].input_sequence_offset) return false;
			}
		}
		return true;
	}

    bool ValidateT7Semantics(const StructsT7::TKMovesetHeaderBlocks& blocks, const Byte* payload,
        uint64_t size, std::string* error)
    {
        using namespace StructsT7;
        namespace G = StructsT7_gameAddr;
        auto fail = [&](const char* message) { if (error) *error = message; return false; };
        if (error) error->clear();
        if (!ValidateT7Layout(blocks, payload, size)) return fail("Invalid T7 block layout");
        const auto& info = *reinterpret_cast<const G::MovesetInfo*>(payload);
        const auto& table = info.table;
        auto records = payload + blocks.movesetBlock;
        auto moves = std::span(reinterpret_cast<const G::Move*>(records + table.move), table.moveCount);
        auto validMove = [&](uint16_t id) {
            return id < table.moveCount || (id >= 0x8000 && id - 0x8000 < std::size(info.current_aliases));
        };
        if (!moves.empty()) {
            for (auto id : info.orig_aliases) if (id >= table.moveCount) return fail("Invalid original move alias");
            for (auto id : info.current_aliases) if (id >= table.moveCount) return fail("Invalid current move alias");
        }
        for (auto address : {info.character_name_addr, info.character_creator_addr, info.date_addr, info.fulldate_addr}) {
            auto offset = reinterpret_cast<uint64_t>(address);
            if (offset && (offset < blocks.nameBlock || offset >= blocks.movesetBlock ||
                !memchr(payload + offset, 0, blocks.movesetBlock - offset))) return fail("Invalid character metadata string");
        }
        // Reuse the complete production foreign-key checks on a private copy.
        {
            std::vector<Byte> scratch(payload, payload + size);
            if (!ConvertMovesetIndexes(scratch.data(), 0, &table, &blocks)) return fail("Invalid T7 record reference");
        }
        auto lastEnd = [](auto entries, auto predicate) {
            size_t end = 0;
            for (size_t i = 0; i < entries.size(); ++i) if (predicate(entries[i])) end = i + 1;
            return end;
        };
        auto requirements = std::span(reinterpret_cast<const Requirement*>(records + table.requirement), table.requirementCount);
        auto cancels = std::span(reinterpret_cast<const G::Cancel*>(records + table.cancel), table.cancelCount);
        auto groups = std::span(reinterpret_cast<const G::Cancel*>(records + table.groupCancel), table.groupCancelCount);
        auto extra = std::span(reinterpret_cast<const ExtraMoveProperty*>(records + table.extraMoveProperty), table.extraMovePropertyCount);
        auto voices = std::span(reinterpret_cast<const Voiceclip*>(records + table.voiceclip), table.voiceclipCount);
        auto begin = std::span(reinterpret_cast<const G::OtherMoveProperty*>(records + table.moveBeginningProp), table.moveBeginningPropCount);
        auto end = std::span(reinterpret_cast<const G::OtherMoveProperty*>(records + table.moveEndingProp), table.moveEndingPropCount);
        const auto requirementEnd = lastEnd(requirements, [](const auto& r) { return r.condition == 881; });
        const auto cancelEnd = lastEnd(cancels, [](const auto& r) { return r.command == 0x8000; });
        const auto groupEnd = lastEnd(groups, [](const auto& r) { return r.command == 0x800c; });
        // An extra-property list ends on the row whose property id is 0, whatever its starting frame; a frame-0 row
        // with a property id is not an end. Same rule as the T7 editor and the T7 -> T8 converter (buildList).
        const auto extraEnd = lastEnd(extra, [](const auto& r) { return r.id == 0; });
        const auto voiceEnd = lastEnd(voices, [](const auto& r) { return r.id == UINT32_MAX; });
        const auto hits = std::span(reinterpret_cast<const G::HitCondition*>(records + table.hitCondition), table.hitConditionCount);
        std::vector<bool> hitTerminated(hits.size());
        bool reachesEnd = false;
        for (size_t i = hits.size(); i-- > 0;) {
            const auto requirement = hits[i].requirements_addr;
            reachesEnd = requirement < requirements.size() &&
                (requirements[requirement].condition == 881 || reachesEnd);
            hitTerminated[i] = reachesEnd;
        }
        auto validHitList = [&](uint64_t head) {
            return head == UINT64_MAX || (head < hitTerminated.size() && hitTerminated[head]);
        };
        const auto beginEnd = lastEnd(begin, [](const auto& r) { return r.extraprop == 881; });
        const auto endEnd = lastEnd(end, [](const auto& r) { return r.extraprop == 881; });
        auto terminated = [](uint64_t head, uint64_t limit) { return head == UINT64_MAX || head < limit; };
        auto validRequirement = [&](uint64_t head) { return terminated(head, requirementEnd); };
        for (const auto& move : moves) {
            if (!validMove(move.transition)) return fail("Invalid move transition");
            if (!validHitList(move.hit_condition_addr)) return fail("Unterminated hit-condition list");
            if (!terminated(move.cancel_addr, cancelEnd) || !terminated(move._0x28_cancel_addr, cancelEnd) ||
                !terminated(move._0x38_cancel_addr, cancelEnd) || !terminated(move._0x48_cancel_addr, cancelEnd))
                return fail("Unterminated cancel list");
            if (!terminated(move.extra_move_property_addr, extraEnd) ||
                !terminated(move.voicelip_addr, voiceEnd) ||
                !terminated(move.move_start_extraprop_addr, beginEnd) || !terminated(move.move_end_extraprop_addr, endEnd))
                return fail("Unterminated move property or voice list");
        }
        for (const auto& projectile : std::span(reinterpret_cast<const G::Projectile*>(records + table.projectile), table.projectileCount))
            if (!validHitList(projectile.hit_condition_addr) || !terminated(projectile.cancel_addr, cancelEnd))
                return fail("Unterminated projectile list");
        auto validateCancels = [&](auto entries) {
            for (const auto& cancel : entries) {
                if (!validRequirement(cancel.requirements_addr)) return false;
                if (cancel.command == 0x800b) { if (!terminated(cancel.move_id, groupEnd)) return false; }
                else if (cancel.command != 0x8000 && cancel.command != 0x800c && !validMove(cancel.move_id)) return false;
                // This is the editor's existing encoded-input range, retaining the high command bits.
                const auto command = static_cast<uint32_t>(cancel.command);
                if (command >= 0x800d && command < 0x10000 && command - 0x800d >= table.inputSequenceCount) return false;
            }
            return true;
        };
        if (!validateCancels(cancels) || !validateCancels(groups)) return fail("Invalid cancel target, input sequence or requirement list");
        for (const auto& hit : std::span(reinterpret_cast<const G::HitCondition*>(records + table.hitCondition), table.hitConditionCount))
            if (!validRequirement(hit.requirements_addr)) return fail("Unterminated hit-condition requirements");
        for (auto entries : {begin, end})
            for (const auto& property : entries) if (!validRequirement(property.requirements_addr)) return fail("Unterminated property requirements");
        for (const auto& reaction : std::span(reinterpret_cast<const G::Reactions*>(records + table.reactions), table.reactionsCount))
            // The first union element is vertical_pushback, not a move ID.
            for (size_t i = 1; i < std::size(reaction.moveids); ++i)
                if (!validMove(reaction.moveids[i])) return fail("Invalid reaction move");
        for (const auto& sequence : std::span(reinterpret_cast<const G::InputSequence*>(records + table.inputSequence), table.inputSequenceCount)) {
            if (sequence.input_amount && (sequence.input_addr == UINT64_MAX || sequence.input_amount > table.inputCount - sequence.input_addr))
                return fail("Input sequence exceeds its input table");
        }
        // Animation contents are not judged: an animation this tool cannot read may still play (each move's offset
        // is bounded by the layout check). Only the MOTA containers' structure, which the game walks, is checked.
        std::set<uint64_t> motas;
        for (auto offset : info.motas.motas) if (offset != MOVESET_ADDR_MISSING) motas.insert(offset);
        for (auto it = motas.begin(); it != motas.end(); ++it) {
            auto next = std::next(it);
            const auto end = next == motas.end() ? blocks.movelistBlock - blocks.motaBlock : *next;
            const auto extent = end - *it;
            const auto data = payload + blocks.motaBlock + *it;
            if (extent < 20 || memcmp(data, "MOTA", 4)) return fail("Invalid MOTA header");
            TAnimUtils::AnimationReader reader({data, static_cast<size_t>(extent)});
            const bool swapped = data[5] == 1;
            const auto count = reader.Read<uint32_t>(12, swapped);
            if (count > (extent - 20) / 4) return fail("Invalid MOTA offset table");
            for (uint32_t i = 0; i < count; ++i) {
                auto offset = reader.Read<uint32_t>(20 + i * 4ULL, swapped);
                if (offset < 20ULL + count * 4ULL || offset >= extent) return fail("Invalid MOTA animation offset");
            }
        }
        const auto trainingSize = size - blocks.movelistBlock;
        if (trainingSize >= 4 && !memcmp(payload + blocks.movelistBlock, "MVLT", 4)) {
            auto head = reinterpret_cast<const MvlHead*>(payload + blocks.movelistBlock);
            const auto base = reinterpret_cast<const Byte*>(head);
            auto rows = reinterpret_cast<const MvlDisplayable*>(base + head->displayables_offset);
            auto playables = reinterpret_cast<const MvlPlayable*>(base + head->playables_offset);
            uint64_t usedInputs = 0;
            for (int i = 0; i < head->playables_count; ++i)
                if (playables[i].input_count)
                    usedInputs = (std::max)(usedInputs, uint64_t(playables[i].input_sequence_offset) + playables[i].input_count);
            std::vector<std::pair<uint64_t, uint64_t>> ranges;
            auto tableRange = [&](uint64_t offset, uint64_t bytes) {
                if (!bytes) return true;
                if (offset < offsetof(MvlHead, translation_strings)) return false;
                ranges.emplace_back(offset, offset + bytes);
                return true;
            };
            if (!tableRange(head->displayables_offset, uint64_t(head->displayables_count) * sizeof(MvlDisplayable)) ||
                !tableRange(head->playables_offset, uint64_t(head->playables_count) * sizeof(MvlPlayable)) ||
                !tableRange(head->inputs_offset, usedInputs * sizeof(MvlInput))) return fail("Invalid movelist table start");
            std::sort(ranges.begin(), ranges.end());
            for (size_t i = 1; i < ranges.size(); ++i)
                if (ranges[i].first < ranges[i - 1].second) return fail("Overlapping movelist tables");
            for (int i = 0; i < head->displayables_count; ++i) {
                for (auto offset : rows[i].all_translation_offsets) {
                    if (!offset) continue; // Uninitialized translation fields have no string yet.
                    const auto terminator = static_cast<const Byte*>(memchr(base + offset, 0, trainingSize - offset));
                    if (!terminator) return fail("Unterminated movelist string");
                    const auto length = static_cast<uint64_t>(terminator - (base + offset)) + 1;
                    if (offset < offsetof(MvlHead, translation_strings)) return fail("Movelist string overlaps header");
                    for (const auto& [start, end] : ranges)
                        if (uint64_t(offset) < end && start < uint64_t(offset) + length)
                            return fail("Movelist string overlaps records");
                }
            }
            for (int i = 0; i < head->displayables_count; ++i)
                if (rows[i].playable_id != UINT16_MAX && rows[i].playable_id >= head->playables_count)
                    return fail("Invalid movelist playable target");
        }
        return true;
    }

	void ConvertDisplayableMovelistOffsets(StructsT7::MvlHead* mvlHead)
	{
		StructsT7::MvlDisplayable* displayable = (StructsT7::MvlDisplayable*)((uint64_t)mvlHead + mvlHead->displayables_offset);
		for (size_t i = 0; i < mvlHead->displayables_count; ++i)
		{
			int32_t absoluteDisplayableOffset = mvlHead->displayables_offset + (int32_t)(i * sizeof(StructsT7::MvlDisplayable));
			for (int j = 0; j < _countof(displayable->all_translation_offsets); ++j) {
				int32_t correctedOffset = displayable->all_translation_offsets[j] - absoluteDisplayableOffset;
				displayable->all_translation_offsets[j] = correctedOffset;
			}
			++displayable;
		}

		StructsT7::MvlPlayable* playable = (StructsT7::MvlPlayable*)((uint64_t)mvlHead + mvlHead->playables_offset);
		for (size_t i = 0; i < mvlHead->playables_count; ++i)
		{
			uint32_t playable_addr = mvlHead->playables_offset + (int32_t)(sizeof(StructsT7::MvlPlayable) * i);
			uint32_t input_sequence_id = playable->input_sequence_offset;
			uint32_t input_sequence_addr = input_sequence_id * sizeof(StructsT7::MvlInput) + mvlHead->inputs_offset;

			playable->input_sequence_offset = input_sequence_addr - playable_addr;
			++playable;
		}
	}


	void ConvertMovesetTableOffsets(const StructsT7::TKMovesetHeaderBlocks* offsets, Byte* moveset, gameAddr gameMoveset)
	{
		StructsT7_gameAddr::MovesetTable* table = (StructsT7_gameAddr::MovesetTable*)(moveset + offsets->tableBlock);
		gameAddr offset = gameMoveset + offsets->movesetBlock;

		table->reactions += offset;
		table->requirement += offset;
		table->hitCondition += offset;
		table->projectile += offset;
		table->pushback += offset;
		table->pushbackExtradata += offset;
		table->cancel += offset;
		table->groupCancel += offset;
		table->cancelExtradata += offset;
		table->extraMoveProperty += offset;
		table->moveBeginningProp += offset;
		table->moveEndingProp += offset;
		table->move += offset;
		table->voiceclip += offset;
		table->inputSequence += offset;
		table->input += offset;
		table->unknownParryRelated += offset;
		table->cameraData += offset;
		table->throwCameras += offset;
	}


	bool ConvertMovesetIndexes(Byte* moveset, gameAddr gameMoveset, const StructsT7_gameAddr::MovesetTable* table, const StructsT7::TKMovesetHeaderBlocks* offsets)
	{
		gameAddr blockOffset = gameMoveset + offsets->movesetBlock;

		for (auto& move : StructIterator<StructsT7_gameAddr::Move>(moveset, offsets->movesetBlock + table->move, table->moveCount))
		{
			move.name_addr += gameMoveset + offsets->nameBlock;
			move.anim_name_addr += gameMoveset + offsets->nameBlock;
			move.anim_addr += gameMoveset + offsets->animationBlock;

			if (!RelocateIndex<StructsT7::Cancel>(move.cancel_addr, blockOffset, table->cancel, table->cancelCount)) return false;
			if (!RelocateIndex<StructsT7::Cancel>(move._0x28_cancel_addr, blockOffset, table->cancel, table->cancelCount)) return false;
			if (!RelocateIndex<StructsT7::Cancel>(move._0x38_cancel_addr, blockOffset, table->cancel, table->cancelCount)) return false;
			if (!RelocateIndex<StructsT7::Cancel>(move._0x48_cancel_addr, blockOffset, table->cancel, table->cancelCount)) return false;
			if (!RelocateIndex<StructsT7::HitCondition>(move.hit_condition_addr, blockOffset, table->hitCondition, table->hitConditionCount)) return false;
			if (!RelocateIndex<StructsT7::Voiceclip>(move.voicelip_addr, blockOffset, table->voiceclip, table->voiceclipCount)) return false;
			if (!RelocateIndex<StructsT7::ExtraMoveProperty>(move.extra_move_property_addr, blockOffset, table->extraMoveProperty, table->extraMovePropertyCount)) return false;
			if (!RelocateIndex<StructsT7::OtherMoveProperty>(move.move_start_extraprop_addr, blockOffset, table->moveBeginningProp, table->moveBeginningPropCount)) return false;
			if (!RelocateIndex<StructsT7::OtherMoveProperty>(move.move_end_extraprop_addr, blockOffset, table->moveEndingProp, table->moveEndingPropCount)) return false;
		}

		// Convert projectile ptrs
		for (auto& projectile : StructIterator<StructsT7_gameAddr::Projectile>(moveset, offsets->movesetBlock + table->projectile, table->projectileCount))
		{
			// One projectile actually has both at NULL for some reason ? todo : check
			if (!RelocateIndex<StructsT7::Cancel>(projectile.cancel_addr, blockOffset, table->cancel, table->cancelCount)) return false;
			if (!RelocateIndex<StructsT7::HitCondition>(projectile.hit_condition_addr, blockOffset, table->hitCondition, table->hitConditionCount)) return false;
		}

		// Convert cancel ptrs
		for (auto& cancel : StructIterator<StructsT7_gameAddr::Cancel>(moveset, offsets->movesetBlock + table->cancel, table->cancelCount))
		{
			if (!RelocateIndex<StructsT7::Requirement>(cancel.requirements_addr, blockOffset, table->requirement, table->requirementCount)) return false;
			if (!RelocateIndex<StructsT7::CancelExtradata>(cancel.extradata_addr, blockOffset, table->cancelExtradata, table->cancelExtradataCount)) return false;
		}

		// Convert groupe dcancel ptrs
		for (auto& groupCancel : StructIterator<StructsT7_gameAddr::Cancel>(moveset, offsets->movesetBlock + table->groupCancel, table->groupCancelCount))
		{
			if (!RelocateIndex<StructsT7::Requirement>(groupCancel.requirements_addr, blockOffset, table->requirement, table->requirementCount)) return false;
			if (!RelocateIndex<StructsT7::CancelExtradata>(groupCancel.extradata_addr, blockOffset, table->cancelExtradata, table->cancelExtradataCount)) return false;
		}

		// Convert reaction ptrs
		for (auto& reaction : StructIterator<StructsT7_gameAddr::Reactions>(moveset, offsets->movesetBlock + table->reactions, table->reactionsCount))
		{
			if (!RelocateIndex<StructsT7::Pushback>(reaction.front_pushback, blockOffset, table->pushback, table->pushbackCount)) return false;
			if (!RelocateIndex<StructsT7::Pushback>(reaction.backturned_pushback, blockOffset, table->pushback, table->pushbackCount)) return false;
			if (!RelocateIndex<StructsT7::Pushback>(reaction.left_side_pushback, blockOffset, table->pushback, table->pushbackCount)) return false;
			if (!RelocateIndex<StructsT7::Pushback>(reaction.right_side_pushback, blockOffset, table->pushback, table->pushbackCount)) return false;
			if (!RelocateIndex<StructsT7::Pushback>(reaction.front_counterhit_pushback, blockOffset, table->pushback, table->pushbackCount)) return false;
			if (!RelocateIndex<StructsT7::Pushback>(reaction.downed_pushback, blockOffset, table->pushback, table->pushbackCount)) return false;
			if (!RelocateIndex<StructsT7::Pushback>(reaction.block_pushback, blockOffset, table->pushback, table->pushbackCount)) return false;
		}

		// Convert input sequence ptrs
		for (auto& inputSequence : StructIterator<StructsT7_gameAddr::InputSequence>(moveset, offsets->movesetBlock + table->inputSequence, table->inputSequenceCount))
		{
			if (!RelocateIndex<StructsT7::Input>(inputSequence.input_addr, blockOffset, table->input, table->inputCount)) return false;
		}

		// Convert throwCameras ptrs
		for (auto& throwCameras : StructIterator<StructsT7_gameAddr::ThrowCamera>(moveset, offsets->movesetBlock + table->throwCameras, table->throwCamerasCount))
		{
			if (!RelocateIndex<StructsT7::CameraData>(throwCameras.cameradata_addr, blockOffset, table->cameraData, table->cameraDataCount)) return false;
		}

		// Convert hit conditions ptrs
		for (auto& hitCondition : StructIterator<StructsT7_gameAddr::HitCondition>(moveset, offsets->movesetBlock + table->hitCondition, table->hitConditionCount))
		{
			if (!RelocateIndex<StructsT7::Requirement>(hitCondition.requirements_addr, blockOffset, table->requirement, table->requirementCount)) return false;
			if (!RelocateIndex<StructsT7::Reactions>(hitCondition.reactions_addr, blockOffset, table->reactions, table->reactionsCount)) return false;
		}

		// Convert pushback ptrs
		for (auto& pushback : StructIterator<StructsT7_gameAddr::Pushback>(moveset, offsets->movesetBlock + table->pushback, table->pushbackCount))
		{
			if (!RelocateIndex<StructsT7::PushbackExtradata>(pushback.extradata_addr, blockOffset, table->pushbackExtradata, table->pushbackExtradataCount)) return false;
		}

		// Convert move-start ptrs
		for (auto& moveBeginProp : StructIterator<StructsT7_gameAddr::OtherMoveProperty>(moveset, offsets->movesetBlock + table->moveBeginningProp, table->moveBeginningPropCount))
		{
			if (!RelocateIndex<StructsT7::Requirement>(moveBeginProp.requirements_addr, blockOffset, table->requirement, table->requirementCount)) return false;
		}

		// Convert move-end prop ptrs
		for (auto& moveEndProp : StructIterator<StructsT7_gameAddr::OtherMoveProperty>(moveset, offsets->movesetBlock + table->moveEndingProp, table->moveEndingPropCount))
		{
			if (!RelocateIndex<StructsT7::Requirement>(moveEndProp.requirements_addr, blockOffset, table->requirement, table->requirementCount)) return false;
		}
		return true;
	}


	void CorrectMovesetInfoValues(StructsT7::MovesetInfo* info, gameAddr gameMoveset)
	{
		info->character_name_addr += gameMoveset;
		info->character_creator_addr += gameMoveset;
		info->date_addr += gameMoveset;
		info->fulldate_addr += gameMoveset;
	}


	void EnforceCurrentAliasesAsDefault(Byte* moveset)
	{
		StructsT7::MovesetInfo* infos = (StructsT7::MovesetInfo*)moveset;

		for (int i = 0; i < _countof(infos->current_aliases); ++i) {
			infos->orig_aliases[i] = infos->current_aliases[i];
		}
	}


	void EnforceDefaultAliasesAsCurrent(Byte* moveset)
	{
		StructsT7::MovesetInfo* infos = (StructsT7::MovesetInfo*)moveset;

		for (int i = 0; i < _countof(infos->current_aliases); ++i) {
			infos->current_aliases[i] = infos->orig_aliases[i];
		}
	}
}
