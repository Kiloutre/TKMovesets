#pragma once

#include "Editor.hpp"

class EditorWindowBase {
public:
	// Stores important moveset informations such as aliases, etc...
	EditorTable editorTable;

	virtual void OpenFormWindow(EditorWindowType_ windowType, uint16_t moveId) = 0;
	virtual int32_t ValidateMoveId(const char* buf) = 0;
};