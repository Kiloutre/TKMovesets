#pragma once

#include "Editor.hpp"

// Base class used for methods that should be accessible in child windows
class EditorWindowBase {
public:
	// Stores important moveset informations such as aliases, etc...
	EditorTable* editorTable = nullptr;
	// Contains the movelist displayed at all times
	std::vector<DisplayableMove*> movelist;

	virtual void OpenFormWindow(EditorWindowType_ windowType, uint16_t moveId, int listSize = 0) = 0;
	virtual int32_t ValidateMoveId(const char* buf) = 0;
	virtual void ReloadMovelistFilter() = 0;
};