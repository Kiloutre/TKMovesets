#pragma once

#include <map>
#include <vector>
#include <set>
#include <string>
#include <imgui.h>

typedef std::set<ImGuiKey> s_Keybind;
typedef std::map<std::string, s_Keybind> s_Keybinds;

namespace Keybinds
{
	// Returns true if the given key is not allowed to be in any keybind
	bool IsForbiddenKey(ImGuiKey k);

	// Returns the list of default keybinds. Used for resetting & knowing which keybind to save.
	s_Keybinds GetDefaultKeybinds();

	// Returns true if the provided keybind is different from the default one. Used to avoid cluttering up the settings file that saves keybinds.
	bool DifferFromDefault(const s_Keybinds& defaultKeybinds, const std::string& identifier, const s_Keybind&keys);

	// Resets the keybinds and apply the default ones
	void ApplyDefaultKeybinds();

	// Returns a pointer to a keybind identifier string that is being pressed or NULLPTR if none
	const std::string* DetectKeybindPress();

	// Returns a reference to the list of keybinds, used during Settings loading/saving
	s_Keybinds& GetKeybinds();

	// Register a specific keybind in the keybind list
	void RegisterKeybind(const std::string& identifier, const std::string& keys);
}