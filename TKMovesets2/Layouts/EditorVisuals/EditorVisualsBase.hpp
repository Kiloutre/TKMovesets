#pragma once

#include "Editor.hpp"
#include "EditorLabel.hpp"

class EditorVisualsBase
{
public:
	// Contains the labels to display in the editor
	EditorLabel* labels = nullptr;

	// Create a new window containing data about the given move. Can be called by subwidnows.
	virtual void OpenFormWindow(EditorWindowType_ windowType, uint16_t structId, int listSize = 0) = 0;
	// Issue an integer field update by adding 'valueChange' to the existing field's value (if not errored).
	virtual void IssueFieldUpdate(EditorWindowType_ winType, int valueChange, int listStart=-1, int listEnd = -1) = 0;
};