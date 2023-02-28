#pragma once

class Submenu_Extract {
public:
	void Render();

	// Whether to overwrite movesets that have already been extracted or not
	bool overwrite_same_filename{ false };
};