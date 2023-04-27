#include <curlpp/cURLpp.hpp>
#include <curlpp/Options.hpp>
#include <sstream>
#include <regex>
#include <fstream>

#include "NavigationMenu.hpp"
#include "helpers.hpp"

#include "constants.h"

// -- Private methods -- //


void NavigationMenu::CheckForUpdates()
{
	DEBUG_LOG("::CheckForUpdates()\n");
	bool updatedAnything = false;

	std::string url = m_addresses->GetString("global", "addresses_url");

	std::ostringstream os;
	std::string content;
	DEBUG_LOG("aa\n");
	try {
		os << curlpp::options::Url(url);
		content = os.str();
	}
	catch (curlpp::LibcurlRuntimeError& e) {
		m_updateStatus.error = true;
		DEBUG_LOG("!! CURL ERROR !!\n");
	}
	DEBUG_LOG("bb\n");

	if (!m_updateStatus.error)
	{
		// Check for addresses update first
		{
			std::smatch m;
			std::regex expr("^ *val:global_addr_version *= *(\\d+) *$");
			int addrVersion = -1;
			int currVersion = m_addresses->GetValue("global", "addr_version");

			while (std::regex_search(content, m, expr)) {
				addrVersion = std::stoi(m[1].str());
				break;
			}

			if (addrVersion != -1 && (addrVersion > currVersion)) {
				DEBUG_LOG("!! writing file... !!\n");

				std::ofstream addrFile(GAME_ADDRESSES_FILE);
				if (addrFile.fail()) {
					DEBUG_LOG("Updater: Failed to write addresses file.\n");
				}
				else {
					addrFile << content;
					addrFile.close();
				}

				m_updateStatus.addrFile = true;
				updatedAnything = true;

				m_addresses->Reload();
			}
			else {
				m_updateStatus.addrFile = false;
			}
		}
	}
	else {
		m_updateStatus.addrFile = false;
		m_updateStatus.programUpdateAvailable = false;
	}

	m_updateStatus.verifying = false;
	m_updateStatus.up_to_date = !updatedAnything;

	DEBUG_LOG("::CheckForUpdates() - END\n");
}


void NavigationMenu::RequestCheckForUpdates()
{
	if (m_addresses == nullptr || m_updateStatus.verifying) {
		return;
	}
	if (m_updateStatus.verifiedOnce) {
		m_updateStatus.thread.join();
	}

	// Reload addresses in case the loaded game addresses is not up to date but the local file one is
	m_addresses->Reload();

	m_updateStatus.verifiedOnce = true;
	m_updateStatus.error = false;
	m_updateStatus.addrFile = false;
	m_updateStatus.up_to_date = false;

	m_updateStatus.verifying = true;
	// Start thread to avoid HTTP thread hanging the display thread
	m_updateStatus.thread = std::thread(&NavigationMenu::CheckForUpdates, this);
}


void NavigationMenu::SetAddrFile(GameAddressesFile* addresses)
{
	m_addresses = addresses;
	RequestCheckForUpdates();
}