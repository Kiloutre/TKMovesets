#include "Editor_t7.hpp"

std::vector<TEditor::MoveCancelReference> EditorT7::ListMoveCancelsReferences(unsigned int moveid) const
{
	std::vector<TEditor::MoveCancelReference> references;

	unsigned int ref_id = 0;
	unsigned int list_start = 0;
	for (const auto& ref : m_iterators.cancels)
	{
		if (!IsCommandInputSequence(ref.command) && !IsCommandGroupedCancelReference(ref.command))
		{
			if (ref.move_id == moveid)
			{
				std::vector<TEditor::MoveCancelReferenceConditions> conditions;

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

	return references;
}

std::vector<TEditor::MoveCancelReference> EditorT7::ListMoveGroupedCancelsReferences(unsigned int moveid) const
{
	std::vector<TEditor::MoveCancelReference> references;

	unsigned int ref_id = 0;
	unsigned int list_start = 0;
	for (const auto& ref : m_iterators.grouped_cancels)
	{
		if (ref.move_id == moveid)
		{
			std::vector<TEditor::MoveCancelReferenceConditions> conditions;

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
				.detection_start = std::to_string(ref.detection_start),
				.detection_end = std::to_string(ref.detection_end),
				.starting_frame = std::to_string(ref.starting_frame),
				.conditions = conditions
			});
		}
		++ref_id;
		if (ref.command == constants.at(EditorConstants_CancelCommandEnd)) {
			list_start = ref_id;
		}
	}
	return references;
}

std::vector<TEditor::MoveReactionsReference> EditorT7::ListMoveReactionsReferences(unsigned int moveid) const
{
	std::vector<TEditor::MoveReactionsReference> references;

	unsigned int ref_id = 0;
	for (const auto& ref : m_iterators.reactions)
	{
		for (auto m : ref.moveids)
		{
			if (m == moveid)
			{
				references.push_back({
					.id = ref_id,
					.id_str = std::to_string(ref_id),
				});
			}
		}
		++ref_id;
	}

	return references;
}