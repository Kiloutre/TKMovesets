#include <set>

#include "Editor_t7.hpp"

// -- Cancels -- //

std::vector<TEditor::CancelMoveReference> EditorT7::ListCancelsMoveReferences(unsigned int cancel_start_id) const
{
	std::vector<TEditor::CancelMoveReference> references;
	unsigned int cancel_end_id = cancel_start_id;
	while (m_iterators.cancels[cancel_end_id]->command != constants.at(EditorConstants_CancelCommandEnd)) {
		++cancel_end_id;
	}

	unsigned int ref_id = 0;
	for (const auto& move : m_iterators.moves)
	{
		// List moves that use this cancel list, even if only a part of it 
		if (
			(cancel_start_id <= move.cancel_addr && move.cancel_addr <= cancel_end_id) ||
			(cancel_start_id <= move._0x28_cancel_addr && move._0x28_cancel_addr <= cancel_end_id) ||
			(cancel_start_id <= move._0x38_cancel_addr && move._0x38_cancel_addr <= cancel_end_id) ||
			(cancel_start_id <= move._0x48_cancel_addr && move._0x48_cancel_addr <= cancel_end_id)
			) {
			references.push_back({
				.move_id = ref_id,
				.name = std::format("{} - {}", ref_id, (char*)(m_movesetData + m_offsets->nameBlock + move.name_addr))
			});
		}
		++ref_id;
	}

	return references;
}

std::vector<TEditor::CancelProjectileReference> EditorT7::ListCancelsProjectilesReferences(unsigned int cancel_start_id) const
{
	std::vector<TEditor::CancelProjectileReference> references;

	unsigned int cancel_end_id = cancel_start_id;
	while (m_iterators.cancels[cancel_end_id]->command != constants.at(EditorConstants_CancelCommandEnd)) {
		++cancel_end_id;
	}

	unsigned int ref_id = 0;
	for (const auto& projectile : m_iterators.projectiles)
	{
		// List projectiles that use this cancel list, even if only a part f it 
		if (cancel_start_id <= projectile.cancel_addr && projectile.cancel_addr <= cancel_end_id) {
			references.push_back({
				.id = ref_id,
				.id_str = std::to_string(ref_id)
			});
		}
		++ref_id;
	}

	struct s_extra_prop_ref {
		unsigned int list_start_id;
		unsigned int id;
		TEditor::CancelProjectileReference& projectile;
	} ;

	std::vector<s_extra_prop_ref> extra_properties_refs;
	unsigned int list_start_id = 0;
	ref_id = 0;
	for (const auto& extra_property : m_iterators.extra_move_properties)
	{
		if (extra_property.id == constants.at(EditorConstants_ProjectileProperty))
		{
			for (auto& projectile : references)
			{
				if (projectile.id == extra_property.value_unsigned)
				{
					extra_properties_refs.push_back({ 
						.list_start_id = list_start_id,
						.id = ref_id,
						.projectile = projectile
					});
				}
			}
		}
		++ref_id;
		if (extra_property.id == constants.at(EditorConstants_ExtraPropertyEnd)) {
			list_start_id = ref_id;
		}
	}

	ref_id = 0;
	for (const auto& move : m_iterators.moves)
	{
		for (auto& prop : extra_properties_refs)
		{
			if (prop.list_start_id <= move.extra_move_property_addr && move.extra_move_property_addr <= prop.id)
			{
				prop.projectile.moves_references.push_back({
					.move_id = ref_id,
					.name = std::format("{} - {}", ref_id, (char*)(m_movesetData + m_offsets->nameBlock + move.name_addr))
				});
			}
		}
		++ref_id;
	}

	return references;
}


// -- Moves -- //

std::vector<TEditor::MoveCancelReference> EditorT7::ListMoveCancelsReferences(unsigned int moveid) const
{
	std::vector<TEditor::MoveCancelReference> references;

	unsigned int ref_id = 0;
	unsigned int list_start = 0;
	for (const auto& ref : m_iterators.cancels)
	{
		// List cancels that use [moveid]
		if (!IsCommandGroupedCancelReference(ref.command))
		{
			if (ref.move_id == moveid)
			{
				std::vector<TEditor::MoveCancelReference::Requirement> conditions;

				for (unsigned int cond_idx = (unsigned int)ref.requirements_addr;
					m_iterators.requirements[cond_idx]->condition != constants.at(EditorConstants_RequirementEnd);
					++cond_idx) {
					const auto& req = m_iterators.requirements[cond_idx];
					if (req->condition != 0) {
						conditions.push_back({ .id = req->condition, .value = req->param_unsigned });
					}
				}

				references.push_back({
					.id = ref_id,
					.list_start_id = list_start,
					.list_start_id_str = std::to_string(list_start),
					.command_str = IsCommandInputSequence(ref.command) ? "..." : "[" + GetCommandStr(ref.command) + "]",
					.detection_start = std::to_string(ref.detection_start),
					.detection_end = std::to_string(ref.detection_end),
					.starting_frame = std::to_string(ref.starting_frame),
					.conditions = conditions
				});
			}
		}
		++ref_id;
		if (ref.command == constants.at(EditorConstants_CancelCommandEnd)) {
			list_start = ref_id;
		}
	}

	for (auto& cancel : references)
	{
		// For each cancel that refer [moveid], list moves that use said cancel
		ref_id = 0;
		for (const auto& move : m_iterators.moves)
		{
				if (
					(cancel.list_start_id <= move.cancel_addr && move.cancel_addr <= cancel.id) ||
					(cancel.list_start_id <= move._0x28_cancel_addr && move._0x28_cancel_addr <= cancel.id) ||
					(cancel.list_start_id <= move._0x38_cancel_addr && move._0x38_cancel_addr <= cancel.id) ||
					(cancel.list_start_id <= move._0x48_cancel_addr && move._0x48_cancel_addr <= cancel.id)
					) {
					cancel.move_references.push_back({
						.move_id = ref_id,
						.name = std::format("{} - {}", ref_id, (char*)(m_movesetData + m_offsets->nameBlock + move.name_addr))
					});
				}
			++ref_id;
		}

		// For each cancel that refer [moveid], list projectiles that use said cancel
		ref_id = 0;
		for (const auto& projectile : m_iterators.projectiles)
		{
			if (cancel.list_start_id <= projectile.cancel_addr && projectile.cancel_addr <= cancel.id ) {
				cancel.projectile_references.push_back(ref_id);
			}
			++ref_id;
		}
	}

	return references;
}

std::vector<TEditor::MoveGroupedCancelReference> EditorT7::ListMoveGroupedCancelsReferences(unsigned int moveid) const
{
	std::vector<TEditor::MoveGroupedCancelReference> references;

	unsigned int ref_id = 0;
	unsigned int list_start = 0;
	for (const auto& ref : m_iterators.grouped_cancels)
	{
		// List grouped_cancels that use [moveid]
		if (ref.move_id == moveid)
		{
			std::vector<TEditor::MoveGroupedCancelReference::Requirement> conditions;

			for (unsigned int cond_idx = (unsigned int)ref.requirements_addr;
				m_iterators.requirements[cond_idx]->condition != constants.at(EditorConstants_RequirementEnd);
				++cond_idx) {

				const auto& req = m_iterators.requirements[cond_idx];
				if (req->condition != 0) {
					conditions.push_back({ .id = req->condition, .value = req->param_unsigned });
				}
			}

			references.push_back({
				.id = ref_id,
				.list_start_id = list_start,
				.list_start_id_str = std::to_string(list_start),
				.command_str = IsCommandInputSequence(ref.command) ? "..." : "[" + GetCommandStr(ref.command) + "]",
				.detection_start = std::to_string(ref.detection_start),
				.detection_end = std::to_string(ref.detection_end),
				.starting_frame = std::to_string(ref.starting_frame),
				.conditions = conditions
			});
		}
		++ref_id;
		if (ref.command == constants.at(EditorConstants_GroupedCancelCommandEnd)) {
			list_start = ref_id;
		}
	}

	ref_id = 0;
	list_start = 0;
	for (const auto& ref: m_iterators.cancels)
	{
		if (IsCommandGroupedCancelReference(ref.command))
		{
			// List each cancel that use previously listed grouped cancels
			for (auto& grouped_cancel : references)
			{
				if (grouped_cancel.list_start_id <= ref.move_id && ref.move_id <= grouped_cancel.id)
				{
					grouped_cancel.cancel_references.push_back({
						.id = ref_id,
						.list_start_id = list_start,
						.list_start_id_str = std::to_string(list_start),
						.starting_frame = std::to_string(ref.starting_frame),
					});
				}
			}
		}
		++ref_id;
		if (ref.command == constants.at(EditorConstants_CancelCommandEnd)) {
			list_start = ref_id;
		}
	}


	for (auto& grouped_cancel : references)
	{
		for (auto& cancel : grouped_cancel.cancel_references)
		{
			ref_id = 0;
			for (auto& move : m_iterators.moves)
			{
				if (
					(cancel.list_start_id <= move.cancel_addr && move.cancel_addr <= cancel.id) ||
					(cancel.list_start_id <= move._0x28_cancel_addr && move._0x28_cancel_addr <= cancel.id) ||
					(cancel.list_start_id <= move._0x38_cancel_addr && move._0x38_cancel_addr <= cancel.id) ||
					(cancel.list_start_id <= move._0x48_cancel_addr && move._0x48_cancel_addr <= cancel.id)
					) {
					grouped_cancel.move_references.push_back({
						.move_id = ref_id,
						.name = std::format("{} - {}", ref_id, (char*)(m_movesetData + m_offsets->nameBlock + move.name_addr))
					});
				}
				++ref_id;
			}
		}
	}

	return references;
}

std::vector<TEditor::MoveReactionsReference> EditorT7::ListMoveReactionsReferences(unsigned int moveid) const
{
	std::vector<TEditor::MoveReactionsReference> references;

	// List reactions that use this move
	unsigned int ref_id = 0;
	for (const auto& ref : m_iterators.reactions)
	{
		bool found_move = false;

		for (auto m : ref.moveids) {
			if (m == moveid) {
				found_move = true;
			}
		}

		if (found_move)
		{
			std::vector<unsigned int> situation_ids;
			unsigned int idx = 0;
			for (auto m : ref.moveids) {
				if (m == moveid) {
					situation_ids.push_back(idx);
				}
				++idx;
			}
			references.push_back({
				.id = ref_id,
				.id_str = std::to_string(ref_id),
				.situation_ids = situation_ids
			});
		}
		++ref_id;
	}

	// List moves that use above listed reactions (have to pass through hit conditions for that)
	for (auto& reaction : references)
	{
		std::vector<std::pair<unsigned int, unsigned int>> hit_conditions;

		// List hit conditions that refer to this reaction
		ref_id = 0;
		int list_start = 0;
		for (const auto& hit_condition : m_iterators.hit_conditions)
		{
			if (hit_condition.reactions_addr == reaction.id && list_start != 0) {
				// Ignore list_start == 0 to avoid filling so many moves with reactions references
				hit_conditions.push_back({ list_start, ref_id });
			}
			++ref_id;
			if (m_iterators.requirements[hit_condition.requirements_addr]->condition == constants.at(EditorConstants_RequirementEnd)) {
				list_start = ref_id;
			}
		}

		// List moves that refer to the above hit conditions
		ref_id = 0;
		for (const auto& move : m_iterators.moves)
		{
			for (const auto& [list_start, list_item] : hit_conditions)
			{
				if (list_start <= move.hit_condition_addr && move.hit_condition_addr <= list_item) {
					// Register the move to the reaction's references
					reaction.references.push_back({
						.move_id = ref_id,
						.name = std::format("{} - {}", ref_id, (char*)(m_movesetData + m_offsets->nameBlock + move.name_addr))
					});
					break;
				}
			}
			++ref_id;
		}

		reaction.references_count_str = std::to_string(reaction.references.size());
	}

	return references;
}