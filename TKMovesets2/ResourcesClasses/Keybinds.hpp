#pragma once

#include <map>
#include <vector>
#include <string>
#include <imgui.h>

typedef std::map<std::string, std::vector<ImGuiKey>> s_Keybinds;

namespace Keybinds
{
	s_Keybinds GetDefaultKeybinds();

	// Returns true if the provided keybind is different from the default one. Used to avoid cluttering up the settings file that saves keybinds.
	bool DifferFromDefault(const s_Keybinds& defaultKeybinds, const std::string& identifier, const std::vector<ImGuiKey>&keys);

	// Resets the keybinds and apply the default ones
	void ApplyDefaultKeybinds();

	// Returns a pointer to a keybind identifier string that is being pressed or NULLPTR if none
	const std::string* DetectKeybindPress();

	// Returns a reference to the list of keybinds, used during Settings loading/saving
	const s_Keybinds& GetKeybinds();

	// Register a specific keybind in the keybind list
	void RegisterKeybind(const std::string& identifier, const std::string& keys);
}