#pragma once
#include <ImGui.h>

class Submenu_Extract {
public:
	// Singleton logic
	Submenu_Extract& operator= (const Submenu_Extract&) = delete;
	Submenu_Extract(const Submenu_Extract&) = delete;

	static Submenu_Extract* getInstance() {
		if (!s_instance) s_instance = new Submenu_Extract();
		return s_instance;
	}

	// Menu logic
	void Render();
private:
	//Singleton logic
	static Submenu_Extract* s_instance;

	// Menu logic
	Submenu_Extract();
};