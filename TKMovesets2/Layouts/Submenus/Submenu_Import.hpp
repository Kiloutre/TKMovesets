#pragma once

#include "GameImport.hpp"

class Submenu_Import
{
private:
	// Stores the last error code
	ImportationErrcode_ m_err = ImportationErrcode_Successful;

	// Return the import settings depending on what checkbox the player checked in the menu
	ImportSettings GetImportationSettings();
public:
	Submenu_Import();
	void Render(GameImport& importerHelper);
};