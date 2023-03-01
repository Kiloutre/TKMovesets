#include <ImGui.h>
#include <stdio.h>

#include "StatusBar.hpp"
#include "Localization.hpp"

void StatusBar::Render()
{
	{
		char buf[40];
		sprintf(buf, "%.1f FPS |", 1000.0f / ImGui::GetIO().Framerate);
		ImGui::TextUnformatted(buf);
		ImGui::SameLine();
	}
}
