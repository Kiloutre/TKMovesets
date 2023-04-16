#pragma once

#include "MovesetLoader.hpp"

class MovesetLoaderT7 : public MovesetLoader
{
protected:
	const TCHAR* GetSharedMemoryName() {
		return TEXT("Local\\TKMovesets2T7Mem");
	}

public:
	void PostInit() override;
};
