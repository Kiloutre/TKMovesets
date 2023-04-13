#pragma once

#include "GameImport.hpp"

class Submenu_Import
{
private:
	// Stores the last error code
	ImportationErrcode_ m_err = ImportationErrcode_Successful;

	// If true, force the current player move to end early
	bool m_applyInstantly;
	// If true, old unused movesets will be free whenever a new one is imported
	bool m_freeUnusedMoveset;

	// Return the import settings depending on what checkbox the player checked in the menu
	ImportSettings GetImportationSettings();
public:
	Submenu_Import();
	void Render(GameImport& importerHelper);
};