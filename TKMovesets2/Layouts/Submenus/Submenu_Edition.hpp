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
	// Stores whether the popup is open or not
	bool m_popupOpen = false;
	// Name of the moveset to apply in the rename popup
	char m_newName[256];
	// Filename of the moveset to rename in the rename popup
	std::string m_toRename;

public:
	movesetInfo* Render(LocalStorage& storage);
};