#include "Editor_t7.hpp"

using namespace StructsT7;

bool EditorT7::IsGenericMove(uint16_t moveId) const
{
	for (unsigned int i = 0; i < _countof(m_infos->orig_aliases); ++i) {
		if (m_infos->orig_aliases[i] == moveId || m_infos->current_aliases[i] == moveId) return true;
	}
	return false;
}

// The T7 moveset as BuildCancelGraph reads it
class EditorT7::CancelSource final : public CancelGraphSource
{
	const EditorT7& m_editor;
	const uint64_t m_cancelEnd, m_groupedEnd, m_requirementEnd, m_sequenceStart;
public:
	explicit CancelSource(const EditorT7& editor) : m_editor(editor),
		m_cancelEnd(editor.constants.at(EditorConstants_CancelCommandEnd)),
		m_groupedEnd(editor.constants.at(EditorConstants_GroupedCancelCommandEnd)),
		m_requirementEnd(editor.constants.at(EditorConstants_RequirementEnd)),
		m_sequenceStart(editor.constants.at(EditorConstants_InputSequenceCommandStart)) {}

	uint32_t MoveCount() const override { return uint32_t(m_editor.m_iterators.moves.size()); }
	int64_t ResolveMove(uint32_t moveId) const override { return m_editor.ResolveMoveId(moveId); }
	bool IsGeneric(uint32_t row) const override { return m_editor.IsGenericMove(uint16_t(row)); }
	std::string MoveName(uint32_t row) const override {
		return (const char*)(m_editor.m_movesetData + m_editor.m_offsets->nameBlock + m_editor.m_iterators.moves[row]->name_addr);
	}
	std::pair<int, int> ActiveFrames(uint32_t row) const override {
		const auto* move = m_editor.m_iterators.moves[row];
		return { int(move->first_active_frame), int(move->last_active_frame) };
	}

	CancelGraph::Kind MoveKind(uint32_t row) const override {
		// The move list's flags (EditorT7::ReloadDisplayableMoveList), and its screw icon
		if (m_editor.MoveUsesScrew(uint16_t(row))) return CancelGraph::Kind::Screw;
		const EditorMoveFlags flags = row < m_editor.displayableMovelist.size() ? m_editor.displayableMovelist[row]->flags : 0;
		if (flags & EditorMoveFlags_Throw) return CancelGraph::Kind::Throw;
		if (flags & EditorMoveFlags_Attack) return CancelGraph::Kind::Attack;
		return CancelGraph::Kind::Other;
	}

	CancelGraph::Cancel Describe(const StructsT7_gameAddr::Cancel& cancel, uint32_t cancelId, bool grouped) const {
		CancelGraph::Cancel result{ .command = cancel.command, .grouped = grouped, .cancelId = cancelId };
		if (m_editor.IsCommandInputSequence(cancel.command)) result.inputSequence = int(uint32_t(cancel.command) - m_sequenceStart);
		else result.input = m_editor.GetCommandStr(cancel.command);
		const auto& requirements = m_editor.m_iterators.requirements;
		for (uint64_t i = cancel.requirements_addr; i < requirements.size(); ++i) {
			const auto* req = requirements[i];
			if (req->condition == m_requirementEnd) break;
			if (req->condition != 0) result.requirements.push_back({ req->condition, req->param_signed });
		}
		return result;
	}

	void ForEachCancel(uint32_t row, const std::function<void(uint32_t, CancelGraph::Cancel&&)>& visit) const override {
		const auto& cancels = m_editor.m_iterators.cancels;
		const auto& groupedCancels = m_editor.m_iterators.grouped_cancels;
		for (uint64_t i = m_editor.m_iterators.moves[row]->cancel_addr; i < cancels.size(); ++i) {
			const auto* cancel = cancels[i];
			if (cancel->command == m_cancelEnd) break;
			if (m_editor.IsCommandGroupedCancelReference(cancel->command)) {
				for (uint64_t g = cancel->move_id; g < groupedCancels.size(); ++g) {
					const auto* groupedCancel = groupedCancels[g];
					if (groupedCancel->command == m_groupedEnd) break;
					visit(groupedCancel->move_id, Describe(*groupedCancel, uint32_t(g), true));
				}
			}
			else visit(cancel->move_id, Describe(*cancel, uint32_t(i), false));
		}
	}
};

CancelGraph EditorT7::BuildCancelGraph(uint16_t rootId, int maxDepth, bool incoming, bool hideCommon) const
{
	return ::BuildCancelGraph(CancelSource(*this), rootId, maxDepth, incoming, hideCommon);
}
