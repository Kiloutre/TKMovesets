#pragma once

#include <memory>
#include <string>
#include <vector>

#include "GameProcess.hpp"
#include "GameData.hpp"
#include "GameAddressesFile.hpp"

class OnlineT8;

// Tools > Extra: Tag in retail Tekken 8 Practice. Two fighters are picked for P3 (P1's partner) and P4 (P2's partner);
// the button injects the moveset loader into the running game and turns Tag on: the next Practice entry loads them.
class Submenu_Extra
{
	struct Fighter { std::string label; int id; };
	std::vector<Fighter> m_fighters;
	bool m_namesMissing = false;       // TKMInterface/t8-retail_names.json not found: built-in roster
	bool m_fromArchive = false;        // roster read from the running game's tkdata.bin (final)
	unsigned long long m_rosterCheckedAt = 0;
	int m_p3 = 0, m_p4 = 0;            // indexes in m_fighters (m_p4 also has "none" at 0)

	GameProcess m_process;
	GameData m_game;
	std::unique_ptr<OnlineT8> m_loader;
	int m_pendingCommand = 0;          // TagCommand_ sent once the loader's shared memory is up
	int m_requestId = 0;
	bool m_injectionStarted = false;
	int m_injectionAttempts = 0;
	std::string m_message;

	void LoadFighters();
	bool Attach(GameAddressesFile& addresses);
	void Send(int command);
	void Update();
public:
	Submenu_Extra();
	~Submenu_Extra();
	void Render(GameAddressesFile& addresses);
};
