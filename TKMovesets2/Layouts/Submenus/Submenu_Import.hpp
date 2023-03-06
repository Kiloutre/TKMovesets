#pragma once

#include "GameImport.hpp"

class Submenu_Import
{
private:
	ImportationErrcode m_err;

public:
	void Render(GameImport& importerHelper);
};