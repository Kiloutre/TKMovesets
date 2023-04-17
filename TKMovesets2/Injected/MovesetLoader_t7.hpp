#pragma once

#include "MovesetLoader.hpp"

class MovesetLoaderT7 : public MovesetLoader
{
private:
	const TCHAR* GetSharedMemoryName() override {
		return TEXT("Local\\TKMovesets2T7Mem");
	}

public:
	void PostInit() override;
};
