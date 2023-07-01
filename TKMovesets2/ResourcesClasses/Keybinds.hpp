#pragma once

#include <map>
#include <vector>
#include <string>
#include <imgui.h>

namespace Keybinds
{
	// Resets the keybinds and apply the default ones
	void ApplyDefaultKeybinds();

	// Returns a pointer to a keybind identifier string that is being pressed or NULLPTR if none
	const std::string* DetectKeybindPress();

	// Returns a reference to the list of keybinds, used during Settings loading/saving
	std::map<std::string, std::vector<ImGuiKey>>& GetKeybinds();

	// Register a specific keybind in the keybind list
	void RegisterKeybind(const std::string& identifier, const std::string& keys);
}