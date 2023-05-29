#pragma once

#include "GameExtract.hpp"

class Submenu_Extract
{
private:
	// Will store the last error to display on a popup
	ExtractionErrcode_ m_err = ExtractionErrcode_Successful;
	// Which mota to export (linked to the checkbox list)
	bool m_motaExport[12];
	// Whether to overwrite moveset extracted with similar character names or use number suffixes
	bool m_overwriteSameFilename;
	// Whether to export the displayable movelist shown in training mode or not
	bool m_extractDisplayableMovelist;
	// Whether to extract unsupported MOTA formats (used for debug purposes)
	bool m_extractUnsupportedMotas;

	// Type of compression to apply to the moveset
	unsigned int m_compressionIndex = 0;

	// Returns an int64 containing flags we set through checkboxes in the settings menu
	ExtractSettings GetExtractionSettings();
public:
	Submenu_Extract();
	void RenderSettingsPopup();
	void Render(GameExtract& extractorHelper);
};
