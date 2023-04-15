#pragma once

#include <windows.h>

#include "MovesetLoader.hpp"

class MovesetLoaderT7 : public MovesetLoader
{
private:
	const TCHAR* GetSharedMemoryName() override {
		return TEXT("TKMovesets2\\T7Mem");
	}

public:
	using MovesetLoader::MovesetLoader; // Inherit constructor too
};