#pragma once

#include "GameExtract.hpp"

class Submenu_Extract
{
private:
	// Will store the last error to display on a popup
	ExtractionErrcode m_err;
	// Which mota to export (linked to the checkbox list)
	bool m_motaExport[12]{ false };
	// Whether to overwrite moveset extracted with similar character names or use number suffixes
	bool m_overwriteSameFilename = false;

	// Returns an int64 containing flags we set through checkboxes in the settings menu
	ExtractionOptions::Settings GetExtractionSettings();
public:
	Submenu_Extract();
	void Render(GameExtract& extractorHelper);
};
