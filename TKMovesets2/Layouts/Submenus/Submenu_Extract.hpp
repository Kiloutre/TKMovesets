#pragma once

#include "GameExtract.hpp"
#include "T8DiskExtraction.hpp"

class Submenu_Extract
{
private:
	// Will store the last error to display on a popup
	ExtractionErrcode_ m_err = ExtractionErrcode_Successful;
	// The extractor's own message for that error, shown instead of the generic text when present
	std::string m_errorDetail;
	// Which mota to export (linked to the checkbox list)
	bool m_motaExport[12];
    bool m_panmExport[6]{true,true,false,false,true,false};
    // Read animations the game has not loaded from its tkdata.bin (ExtractSettings_T8_NO_ARCHIVE when off)
    bool m_panmFromArchive = true;
	// Whether to export the displayable movelist shown in training mode or not
	bool m_extractDisplayableMovelist;
	T8DiskExtraction m_diskExtraction;
	// Whether to extract unsupported MOTA formats (used for debug purposes)
	bool m_extractUnsupportedMotas;

	// Type of compression to apply to the moveset
	unsigned int m_compressionIndex = 0;
    unsigned int m_retailCompressionIndex = 0;

	// Returns an int64 containing flags we set through checkboxes in the settings menu
	ExtractSettings GetExtractionSettings(bool retailT8);
public:
	Submenu_Extract();
	void RenderSettingsPopup(bool retailT8 = false);
	void Render(GameExtract& extractorHelper);
};
