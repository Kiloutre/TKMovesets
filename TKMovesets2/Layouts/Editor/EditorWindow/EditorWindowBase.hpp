#pragma once

#include "Editor.hpp"
#include "EditorLabel.hpp"

// Base class used for methods that should be accessible in child windows
class EditorWindowBase {
public:
	// Stores important moveset informations such as aliases, etc...
	EditorTable* editorTable = nullptr;
	// Contains the movelist displayed at all times
	std::vector<DisplayableMove*> movelist;
	// Contains the labels to display in the editor
	EditorLabel* labels = nullptr;

	virtual void OpenFormWindow(EditorWindowType_ windowType, uint16_t moveId, int listSize = 0) = 0;
	virtual int32_t ValidateMoveId(const char* buf) = 0;
	virtual void ReloadMovelistFilter() = 0;
	virtual void IssueFieldUpdate(std::string fieldShortName, int valueChange, int listStart = -1, int listEnd = -1) = 0;
};