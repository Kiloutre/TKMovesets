#pragma once

class GameData
{
private:
	GameData() = default;
	~GameData() = default;
	GameData& operator = (const GameData&) = delete;
	GameData(const GameData&) = delete;

public:
	static GameData& getInstance() {
		// Todo: mutex here or something?
		static GameData s_instance;
		return s_instance;
	}
};