#pragma once

#include "Games.hpp"

class EditorLabel
{
private:
	std::map<int, std::string> m_labels;

	// Loats a file into our labels
	void LoadFile(const char* dataString, const char* name);
public:
	EditorLabel(const GameInfo* gameInfo);

	// Returns a string for the given requirement/property ID
	const char* GetText(int id);
};