#include <curlpp/cURLpp.hpp>
#include <curlpp/Options.hpp>
#include <sstream>

#include "NavigationMenu.hpp"
#include "helpers.hpp"

#include "constants.h"

// -- Private methods -- //

void NavigationMenu::CheckForUpdates()
{
	std::string url = m_addresses->GetString("global", "repo_url");
	url.replace(url.find("github.com"), sizeof("github.com") - 1, "raw.githubusercontent.com");
	url += "/master/TKMovesets2/Resources/game_addresses.ini";

	std::ostringstream os;
	os << curlpp::options::Url(url);
	std::string content = os.str();

	DEBUG_LOG("%s\n", content.c_str());

	m_checking_for_updates = false;
}

void NavigationMenu::RequestCheckForUpdates()
{
	if (m_addresses == nullptr || m_checking_for_updates) {
		return;
	}
	// Reload addresses in case the loaded game addresses is not up to date but the local file one is
	m_addresses->Reload();

	// Start thread to avoid HTTP thread hanging the display thread
	m_checking_for_updates = true;
	update_check_thread = std::thread(&NavigationMenu::CheckForUpdates, this);
}

void NavigationMenu::SetAddrFile(GameAddressesFile* addresses)
{
	m_addresses = addresses;
	RequestCheckForUpdates();
}