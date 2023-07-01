#include "Keybinds.hpp"

#include "constants.h"

s_Keybinds g_keybinds;

namespace Keybinds
{
	s_Keybinds GetDefaultKeybinds()
	{
		s_Keybinds keybinds;

		// https://github.com/ocornut/imgui/blob/docking/imgui.h -> enum ImGuiKey : int
		keybinds["keybind_editor.import_p1"] = { ImGuiKey_F1 };
		keybinds["keybind_editor.import_p2"] = { ImGuiKey_F2 };
		keybinds["keybind_editor.import_both"] = { ImGuiKey_F3 };

		return keybinds;
	}

	bool DifferFromDefault(const s_Keybinds& defaultKeybinds, const std::string& identifier, const std::vector<ImGuiKey>& keys)
	{
		auto item = defaultKeybinds.find(identifier);
		if (item == defaultKeybinds.end()) return true;

		const auto& orig_keys = item->second;

		return orig_keys == keys;
	}

	void ApplyDefaultKeybinds()
	{
		g_keybinds = GetDefaultKeybinds();
	}

	const std::string* DetectKeybindPress()
	{
		for (const auto& [identifier, keys] : g_keybinds)
		{
			bool any_pressed = true;
			bool new_key_down = false;

			for (const auto& key : keys)
			{
				if (!ImGui::IsKeyDown(key)) {
					any_pressed = false;
					break;
				}
				else if (ImGui::IsKeyPressed(key, false)) {
					new_key_down = true;
				}
			}

			if (any_pressed && new_key_down) {
				DEBUG_LOG("Detected keybind press: '%s'\n", identifier.c_str());
				return &identifier;
			}
		}

		return nullptr;
	}

	const s_Keybinds& GetKeybinds()
	{
		return g_keybinds;
	}

	void RegisterKeybind(const std::string& identifier, const std::string& keys)
	{
		std::vector<ImGuiKey> bind_keys;

		const char* prev_cursor = keys.c_str();
		const char* cursor = strstr(prev_cursor, ",");

		while (cursor != nullptr)
		{
			ImGuiKey k = (ImGuiKey)atoi(prev_cursor);
			bind_keys.push_back(k);
			prev_cursor = cursor + 1;
			cursor = strstr(prev_cursor, ",");
		}

		if (bind_keys.size() != 0) {
			DEBUG_LOG("Registered keybind '%s' - keys '%s'\n", identifier.c_str(), keys.c_str());
			g_keybinds[identifier] = bind_keys;
		}
		else {
			DEBUG_ERR("Attempting to register keybind '%s' empty of keys.", identifier.c_str());
		}
	}
}