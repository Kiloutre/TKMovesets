#pragma once

class EditorSubwindow
{
public:
	// Unique identifier, used to avoid duplicated subwindows when it doesn't make sense
	std::string identifier;

	// Dock ID to apply on the next render, -1 for none
	int nextDockId = -1;


	// Refresh the subwindow and re-focus it
	virtual void Refresh(ImGuiID nextDockId) = 0;
	// Renders a window, returns false if the window is closed
	virtual bool Render() = 0;
};