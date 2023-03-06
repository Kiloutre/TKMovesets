#include <ImGui.h>
#include <stdio.h>

#include "StatusBar.hpp"
#include "Localization.hpp"

void StatusBar::Render()
{
	{
		char buf[32];
		sprintf_s(buf, sizeof(buf), "%.1f FPS |", 1000.0f / ImGui::GetIO().Framerate);
		ImGui::TextUnformatted(buf);
		ImGui::SameLine();
	}
}
