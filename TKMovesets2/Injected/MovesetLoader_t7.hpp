#pragma once

#include "MovesetLoader.hpp"

class MovesetLoaderT7 : public MovesetLoader
{
private:
	const char* GetMainModuleName() override {
		return "TekkenGame-Win64-Shipping.exe";
	}

	const TCHAR* GetSharedMemoryName() override {
		return TEXT("Local\\TKMovesets2T7Mem");
	}

public:
	void PostInit() override;
};
