#pragma once

#include <windows.h>

#include "Online.hpp"

class DLLCONTENT OnlineT7 : public Online
{
private:
	const TCHAR* GetSharedMemoryName() {
		return TEXT("Local\\TKMovesets2T7Mem");
	}
public:
	using Online::Online; // Inherit constructor too
};