#include <cstring>
#include <set>

#include "EditorVisuals_t8Retail.hpp"
#include "Localization.hpp"
#include "MovesetConverters/RetailCommands.hpp"

namespace
{
	template<class T> T At(std::span<const Byte> record, size_t offset) {
		T value{};
		if (offset + sizeof(T) <= record.size()) memcpy(&value, record.data() + offset, sizeof(T));
		return value;
	}

	const EditorT8Retail::Field& SchemaField(size_t table, const char* name) {
		for (const auto& field : EditorT8Retail::Schema(table)) if (field.name == name) return field;
		throw std::logic_error(std::string("Retail schema has no field ") + name);
	}

	// The retail T8 document as BuildCancelGraph reads it (tables 6 cancels, 7 group cancels, 1 requirements, 12 moves;
	// docs/t8-structures.md). Only the main cancel list (+0x98) is followed, as in T7.
	class RetailCancelSource final : public CancelGraphSource
	{
		const EditorVisualsT8Retail& m_view;
		const EditorT8Retail& m_editor;
		const EditorT8Retail::Field& m_moveCancels = SchemaField(12, "cancels");
		const EditorT8Retail::Field& m_cancelRequirements = SchemaField(6, "requirements");
		const EditorT8Retail::Field& m_groupRequirements = SchemaField(7, "requirements");
		std::set<uint32_t> m_aliasTargets;
	public:
		RetailCancelSource(const EditorVisualsT8Retail& view, const EditorT8Retail& editor) : m_view(view), m_editor(editor) {
			for (uint32_t alias = 0x8000; alias < 0x8000 + 60; ++alias)
				try { m_aliasTargets.insert(m_editor.ResolveMove(alias)); } catch (const std::exception&) {}
		}
		uint32_t MoveCount() const override { return uint32_t(m_editor.Count(12)); }
		int64_t ResolveMove(uint32_t moveId) const override {
			try { return m_editor.ResolveMove(moveId); } catch (const std::exception&) { return -1; }
		}
		bool IsGeneric(uint32_t row) const override { return m_aliasTargets.contains(row); }
		std::string MoveName(uint32_t row) const override {
			if (auto hash = m_editor.MoveNameHash(row)) return m_view.NameForHash(*hash);
			return _("edition.t8.unknown_move");
		}
		std::pair<int, int> ActiveFrames(uint32_t row) const override {
			const auto record = m_editor.Record(12, row);
			return { int(At<uint32_t>(record, 0x158)), int(At<uint32_t>(record, 0x15C)) };
		}
		CancelGraph::Kind MoveKind(uint32_t row) const override {
			// As the move list (EditorVisualsT8Retail::RebuildBrowser): tornado icon, name ending in _y, hit level with
			// an active hitbox slot
			if (m_editor.MoveUsesTornado(row)) return CancelGraph::Kind::Screw;
			if (MoveName(row).ends_with("_y")) return CancelGraph::Kind::Throw;
			const auto record = m_editor.Record(12, row);
			const auto hit = EditorT8Retail::Decode(At<uint64_t>(record, 0x78), At<uint64_t>(record, 0x80)).value_or(0);
			bool active = false;
			for (unsigned box = 0; box < 8; ++box)
				active |= At<uint32_t>(record, 0x160 + box * 0x30) && At<uint32_t>(record, 0x164 + box * 0x30) && At<uint32_t>(record, 0x168 + box * 0x30);
			return hit && active ? CancelGraph::Kind::Attack : CancelGraph::Kind::Other;
		}
		bool ExternalCancels(uint32_t row) const override {
			return At<uint64_t>(m_editor.Record(12, row), m_moveCancels.offset) != 0 && !m_editor.ReferenceIndex(12, row, m_moveCancels);
		}

		CancelGraph::Cancel Describe(size_t table, uint32_t row, const EditorT8Retail::Field& requirementsField) const {
			const auto record = m_editor.Record(table, row);
			const uint64_t command = At<uint64_t>(record, 0);
			CancelGraph::Cancel result{ .command = command, .grouped = table == 7, .cancelId = row };
			if (RetailT8Commands::IsInputSequence(command)) result.inputSequence = int(RetailT8Commands::InputSequenceId(command));
			else result.input = RetailT8Commands::CommandString(command);
			if (auto head = m_editor.ReferenceIndex(table, row, requirementsField)) {
				for (uint64_t i = *head; i < m_editor.Count(1); ++i) {
					const auto requirement = m_editor.Record(1, uint32_t(i));
					const uint32_t condition = At<uint32_t>(requirement, 0);
					if (condition == 1100) break;
					if (condition != 0) result.requirements.push_back({ condition, At<int32_t>(requirement, 4) });
				}
			}
			return result;
		}

		void ForEachCancel(uint32_t row, const std::function<void(uint32_t, CancelGraph::Cancel&&)>& visit) const override {
			namespace C = RetailT8Commands;
			const auto head = m_editor.ReferenceIndex(12, row, m_moveCancels);
			if (!head) return; // No list, or one outside the moveset
			const uint64_t cancelCount = m_editor.Count(6), groupCount = m_editor.Count(7);
			for (uint64_t i = *head; i < cancelCount; ++i) {
				const auto cancel = m_editor.Record(6, uint32_t(i));
				const uint64_t command = At<uint64_t>(cancel, 0);
				if (command == C::ListEnd) break;
				const uint16_t target = At<uint16_t>(cancel, 0x24);
				if (command == C::GroupCancel) {
					for (uint64_t g = target; g < groupCount; ++g) {
						const auto grouped = m_editor.Record(7, uint32_t(g));
						if (At<uint64_t>(grouped, 0) == C::GroupEnd) break;
						visit(At<uint16_t>(grouped, 0x24), Describe(7, uint32_t(g), m_groupRequirements));
					}
				}
				else visit(target, Describe(6, uint32_t(i), m_cancelRequirements));
			}
		}
	};
}

CancelGraph EditorVisualsT8Retail::BuildCancelGraph(uint32_t row, int depth, bool incoming, bool hideCommon) const
{
	return ::BuildCancelGraph(RetailCancelSource(*this, *m_editor), row, depth, incoming, hideCommon);
}
