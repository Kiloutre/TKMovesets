#pragma once

class Submenu_Extract {
public:
	void Render();

private:
	// Whether to overwrite movesets that have already been extracted or not
	bool m_overwrite_same_filename{ false };
};