#pragma once

#include "GameImport.hpp"
#include "LocalStorage.hpp"

enum RenameErrcode_
{
	RenameErrcode_NoErr,
	RenameErrcode_EmptyName,
	RenameErrcode_AlreadyExists,
	RenameErrcode_RenameErr,
	RenameErrcode_InvalidName,
};

class Submenu_Edition
{
private:
	// Current error message in rename popup
	RenameErrcode_ currentErr = RenameErrcode_NoErr;
	// Stores whether the rename popup is open or not
	bool m_renamePopup = false;
	// Stores whether the conversion popup is open or not
	bool m_conversionPopup = false;
	// Name of the moveset to apply in the rename popup
	char m_newName[FILENAME_MAX];
	// Moveset to rename/convert in the popups
	movesetInfo m_actionTarget;

	// Render the popup used for moveset renaming
	void RenderRenamePopup();
	// Render the popup used for moveset conversion
	void RenderConversionPopup();
public:
	movesetInfo* Render(LocalStorage& storage);
};