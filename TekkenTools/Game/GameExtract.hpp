#pragma once

class GameExtract
{
private:
	GameExtract() = default;
	~GameExtract() = default;
	GameExtract& operator = (const GameExtract&) = delete;
	GameExtract(const GameExtract&) = delete;

	void ExtractP1ayer(void *playerAddress);
public:
	static GameExtract& getInstance() {
		// Todo: mutex here or something?
		static GameExtract s_instance;
		return s_instance;
	}

	bool busy{ false };
	float extractionProgress{ 0.0f };

	void ExtractP1();
	void ExtractP2();
	void ExtractAll();
};