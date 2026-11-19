#pragma once

#include "GameImport.hpp"

class Submenu_Import
{
private:
	// Stores the last error code
	ImportationErrcode_ m_err = ImportationErrcode_Successful;
    std::string m_errorDetail;
	// Warning of the last successful importation (shown in its own popup)
	std::string m_warning;
	bool m_motaImport[12];
	// Moveset list filter: shows the movesets whose name, game or character contains it (case-insensitive)
	char m_search[128] = {};
	void RenderMotaSettings();

	// Return the import settings depending on what checkbox the player checked in the menu
	ImportSettings GetImportationSettings();
public:
	Submenu_Import();
	void Render(GameImport& importerHelper);
};
