#pragma once
#include <ImGui.h>
#include "Localization.h"

class Submenu_Import {
public:
	// Singleton logic
	Submenu_Import& operator= (const Submenu_Import&) = delete;
	Submenu_Import(const Submenu_Import&) = delete;

	static Submenu_Import* getInstance() {
		if (!s_instance) s_instance = new Submenu_Import();
		return s_instance;
	}

	// Menu logic
	void Render();
private:
	//Singleton logic
	static Submenu_Import* s_instance;

	// Menu logic
	Submenu_Import();
};