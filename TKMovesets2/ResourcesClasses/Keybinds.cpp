#include "Keybinds.hpp"

#include "constants.h"

s_Keybinds g_keybinds;

namespace Keybinds
{
	bool IsForbiddenKey(ImGuiKey k)
	{
		switch (k)
		{
		case ImGuiKey_MouseLeft:
		case ImGuiKey_MouseRight:
			return true;
		default:
			return !(ImGuiKey_NamedKey_BEGIN <= k && k <= ImGuiKey_NamedKey_END);
		}
	}

	s_Keybinds GetDefaultKeybinds()
	{
		s_Keybinds keybinds;

		// https://github.com/ocornut/imgui/blob/docking/imgui.h -> enum ImGuiKey : int
		keybinds["keybind_editor.import_p1"] = { ImGuiKey_F1 };
		keybinds["keybind_editor.import_p2"] = { ImGuiKey_F2 };
		keybinds["keybind_editor.import_both"] = { ImGuiKey_F3 };
		keybinds["keybind_editor.save"] = { ImGuiKey_LeftCtrl, ImGuiKey_S };
		keybinds["keybind_editor.change_data_type"] = { ImGuiKey_LeftCtrl, ImGuiKey_B };

		return keybinds;
	}

	bool DifferFromDefault(const s_Keybinds& defaultKeybinds, const std::string& identifier, const s_Keybind& keys)
	{
		auto item = defaultKeybinds.find(identifier);
		if (item == defaultKeybinds.end()) return true;

		const auto& orig_keys = item->second;

		return orig_keys != keys;
	}

	void ApplyDefaultKeybinds()
	{
		DEBUG_LOG("ApplyDefaultKeybinds\n");
		g_keybinds = GetDefaultKeybinds();
	}

	bool IsKeybindPressed(const char* identifier)
	{
		const auto& keys = g_keybinds[identifier];

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
			DEBUG_LOG("IsKeybindPressed: '%s' = true\n", identifier);
			return true;
		}
		return false;
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

	s_Keybinds& GetKeybinds()
	{
		return g_keybinds;
	}

	void RegisterKeybind(const std::string& identifier, const std::string& keys)
	{
		DEBUG_LOG("RegisterKeybind '%s' '%s'\n", identifier.c_str(), keys.c_str());
		s_Keybind bind_keys;

		const char* prev_cursor = keys.c_str();
		const char* cursor = strstr(prev_cursor, ";");

		while (cursor != nullptr)
		{
			ImGuiKey k = (ImGuiKey)atoi(prev_cursor);
			if (!bind_keys.contains(k) && !IsForbiddenKey(k)) {
				bind_keys.insert(k);
			}
			else {
				DEBUG_ERR("Keybind contains the same key multiple times OR forbidden key. (%u)", k);
			}
			prev_cursor = cursor + 1;
			cursor = strstr(prev_cursor, ";");
		}

		ImGuiKey k = (ImGuiKey)atoi(prev_cursor);
		if (!IsForbiddenKey(k)) {
			bind_keys.insert(k);
		}
		else {
			DEBUG_ERR("Keybind contains forbidden key '%u'.", k);
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