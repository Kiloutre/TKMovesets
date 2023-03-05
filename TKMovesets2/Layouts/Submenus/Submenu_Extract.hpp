#pragma once

#include "GameExtract.hpp"

class Submenu_Extract
{
private:
	ExtractionErrcode m_err;

public:
	void Render(GameExtract& extractorHelper);
};
