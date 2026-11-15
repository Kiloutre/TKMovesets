#include "TEditor.hpp"

namespace TEditorUtils
{
	unsigned int GetMoveColorFromFlag(EditorMoveFlags flags)
	{
		if (flags & EditorMoveFlags_Generic) {
			return MOVE_GENERIC;
		}

		if (flags & EditorMoveFlags_CurrentGeneric) {
			return MOVE_CURRENT_GENERIC;
		}

		if (flags & EditorMoveFlags_Custom) {
			return MOVE_CUSTOM;
		}

		if (flags & EditorMoveFlags_Throw) {
			return MOVE_THROW;
		}
		else if (flags & EditorMoveFlags_ThrowReaction) {
			return MOVE_THROWREACTION;
		}

		if (flags & EditorMoveFlags_Attack) {
			return MOVE_ATK;
		}
		else if (flags & EditorMoveFlags_OtherAttack) {
			return MOVE_OTHERATK;
		}

		return 0;
	}
}

TEditor::TEditor(GameProcess& process, GameData& game, const GameInfo* gameInfo) : EditorLogic(process, game, gameInfo)
{
}

TEditor::~TEditor()
{
	for (auto& move : displayableMovelist) {
		delete move;
	}
}
// The editor works on cloned rows while observers retain the original row pointers.
// Rollback swaps the originals back without allocating, including nested transactions.
class TEditor::SavedMutationState final : public EditorLogic::MutationState
{
    TEditor& editor;
    decltype(m_animNameToOffsetMap) names;
    decltype(m_animOffsetToNameOffset) offsets;
    std::vector<uint16_t> savedAliases;
    std::vector<DisplayableMove*> rows;
    bool hasMovelist, reload;
public:
    explicit SavedMutationState(TEditor& target)
        : editor(target), names(target.m_animNameToOffsetMap), offsets(target.m_animOffsetToNameOffset),
          savedAliases(target.aliases), hasMovelist(target.hasDisplayableMovelist), reload(target.mustReloadMovelist)
    {
        std::vector<std::unique_ptr<DisplayableMove>> copies;
        copies.reserve(target.displayableMovelist.size());
        rows.reserve(target.displayableMovelist.size());
        for (auto row : target.displayableMovelist) copies.push_back(std::make_unique<DisplayableMove>(*row));
        for (auto& row : copies) rows.push_back(row.get());
        rows.swap(target.displayableMovelist);
        for (auto& row : copies) row.release();
    }
    ~SavedMutationState() override { for (auto row : rows) delete row; }
    void Commit() noexcept override { if (!rows.empty() || !editor.displayableMovelist.empty()) editor.mustReloadMovelist = true; }
    void Restore() noexcept override
    {
        names.swap(editor.m_animNameToOffsetMap);
        offsets.swap(editor.m_animOffsetToNameOffset);
        savedAliases.swap(editor.aliases);
        rows.swap(editor.displayableMovelist);
        std::swap(hasMovelist, editor.hasDisplayableMovelist);
        std::swap(reload, editor.mustReloadMovelist);
    }
};
std::unique_ptr<EditorLogic::MutationState> TEditor::CaptureMutationState()
{
    return std::make_unique<SavedMutationState>(*this);
}
