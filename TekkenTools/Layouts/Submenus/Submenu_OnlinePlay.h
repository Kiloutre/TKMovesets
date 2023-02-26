#pragma once
#include <ImGui.h>
#include "Localization.h"

class Submenu_OnlinePlay {
public:
	// Singleton logic
	Submenu_OnlinePlay& operator= (const Submenu_OnlinePlay&) = delete;
	Submenu_OnlinePlay(const Submenu_OnlinePlay&) = delete;

	static Submenu_OnlinePlay* getInstance() {
		if (!s_instance) s_instance = new Submenu_OnlinePlay();
		return s_instance;
	}

	// Menu logic
	void Render();
private:
	//Singleton logic
	static Submenu_OnlinePlay* s_instance;

	// Menu logic
	Submenu_OnlinePlay();
};