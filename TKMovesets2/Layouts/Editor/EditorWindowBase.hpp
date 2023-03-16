#pragma once

#include "Editor.hpp"

class EditorWindowBase {
public:
	virtual void OpenFormWindow(EditorWindowType_ windowType, uint16_t moveId) = 0;
	virtual int32_t ValidateMoveId(const char* buf) = 0;
};