#pragma once

#include "MovesetLoader.hpp"

#include "Structs_t7.h"

class MovesetLoaderT7 : public MovesetLoader
{
private:
	const TCHAR* GetSharedMemoryName() override {
		return TEXT("Local\\TKMovesets2T7Mem");
	}

public:
	void InitHooks() override;
	void PostInit() override;
};
