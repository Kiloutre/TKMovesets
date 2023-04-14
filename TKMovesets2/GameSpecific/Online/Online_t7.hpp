#pragma once

#include <windows.h>

#include "Online.hpp"

class DLLCONTENT OnlineT7 : public Online
{
private:
	const TCHAR* GetSharedMemoryName() {
		return TEXT("TKMovesets2\\T7Mem");
	}
public:
	using Online::Online; // Inherit constructor too
};