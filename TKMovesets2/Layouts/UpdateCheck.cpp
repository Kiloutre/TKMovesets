#include <curlpp/cURLpp.hpp>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/Infos.hpp>
#include <sstream>
#include <regex>
#include <fstream>

#include "NavigationMenu.hpp"
#include "helpers.hpp"

#include "constants.h"

// -- Private methods -- //

static bool VerifyProgramUpdate(bool* error, GameAddressesFile* addresses)
{
	std::string repoUrl = addresses->GetString("global", "repo_url");
	std::string releasesUrl = repoUrl;
	releasesUrl.replace(releasesUrl.find("github.com"), sizeof("github.com") - 1, "api.github.com/repos");
	releasesUrl += "/releases/latest";

	DEBUG_LOG("Querying '%s'...\n", releasesUrl.c_str());

	// Have to set up user agent or github will deny the request
	std::string releasesContent;
	{
		const std::string userAgent = "curl/7.83.1";
		std::list<std::string> headers;
		headers.push_back("Host: api.github.com");
		headers.push_back("Accept: */*");
		headers.push_back("User-Agent: curl/7.83.1");

		curlpp::Cleanup myCleanup;
		curlpp::Easy myRequest;
		myRequest.setOpt(new curlpp::options::Url(releasesUrl));
		myRequest.setOpt(new curlpp::options::HttpHeader(headers));
		myRequest.setOpt(new curlpp::options::Timeout(HTTP_REQUEST_TIMEOUT));

		std::stringstream os;
		try {
			curlpp::options::WriteStream ws(&os);
			myRequest.setOpt(ws);
			myRequest.perform();
			releasesContent = os.str();
		}
		catch (curlpp::LibcurlRuntimeError&) {
			*error = true;
			DEBUG_LOG("!! CURL ERROR !!\n");
			return false;
		}
		catch (curlpp::LogicError&) {
			*error = true;
			DEBUG_LOG("!! CURL ERROR !!\n");
			return false;
		}
	}


	std::smatch m;
	std::regex urlExpr("browser_download_url\" *: *\"([^\"]+)\"");
	std::regex tagExpr("tag_name\" *: *\"([^\"]+)\"");
	std::string downloadUrl;
	std::string tagName;

	if (std::regex_search(releasesContent, m, urlExpr)) {
		downloadUrl = m[1].str();
	}
	if (std::regex_search(releasesContent, m, tagExpr)) {
		tagName = m[1].str();
	}

	DEBUG_LOG("Remote URL: [%s]\n", downloadUrl.c_str());
	DEBUG_LOG("Remote tagname: [%s]\n", tagName.c_str());

	if (downloadUrl.empty() || tagName.empty()) {
		return false;
	}

	if (!Helpers::remoteVersionGreater(tagName.c_str())) {
		return false;
	}

	if (!Helpers::endsWith<std::string>(downloadUrl, ".exe")) {
		// todo: Maybe handle .7z & .zip releases
		DEBUG_LOG("Bad release extension (not .exe), can't install.\n");
		return false;
	}

	// In case other extensions are ever handled, fetch the extension dynamically to write the proper file name
	std::string extension = downloadUrl.substr(downloadUrl.find_last_of("."));

	DEBUG_LOG("New release found. Downloading it.\n");

	// Finally download the file
	{
		const std::string userAgent = "curl/7.83.1";
		std::list<std::string> headers;
		headers.push_back("Host: github.com");
		headers.push_back("Accept: */*");
		headers.push_back("User-Agent: curl/7.83.1");

		curlpp::Cleanup myCleanup;
		curlpp::Easy myRequest;
		myRequest.setOpt(new curlpp::options::FollowLocation(true));
		myRequest.setOpt(new curlpp::options::Url(downloadUrl));
		myRequest.setOpt(new curlpp::options::HttpHeader(headers));
		myRequest.setOpt(new curlpp::options::Timeout(HTTP_REQUEST_TIMEOUT));

		std::wstring filename = L"" UPDATE_TMP_FILENAME;
		filename += Helpers::string_to_wstring(extension);
		try {
			std::ofstream ofs(filename, std::ios::out | std::ios::trunc | std::ios::binary);
			curlpp::options::WriteStream ws(&ofs);
			myRequest.setOpt(ws);
			myRequest.perform();
		}
		catch (curlpp::LibcurlRuntimeError&) {
			*error = true;
			DEBUG_LOG("!! CURL ERROR !!\n");
			return false;
		}
		catch (curlpp::LogicError&) {
			*error = true;
			DEBUG_LOG("!! CURL ERROR !!\n");
			return false;
		}
	}

	return true;
}


static bool UpdateAddresses(bool* error, GameAddressesFile* addresses)
{
	std::string addrUrl = addresses->GetString("global", "repo_url");
	addrUrl.replace(addrUrl.find("github.com"), sizeof("github.com") - 1, "raw.githubusercontent.com");
	addrUrl += "/master/TKMovesets2/Resources/game_addresses.ini";

	DEBUG_LOG("Querying '%s'...\n", addrUrl.c_str());

	std::ostringstream os;
	std::string addrContent;
	try {
		os << curlpp::options::Url(addrUrl);
		addrContent = os.str();
	}
	catch (curlpp::LibcurlRuntimeError&) {
		*error = true;
		DEBUG_LOG("!! CURL ERROR !!\n");
		return false;
	}
	catch (curlpp::LogicError&) {
		*error = true;
		DEBUG_LOG("!! CURL ERROR !!\n");
		return false;
	}

	std::smatch m;
	std::regex expr("^ *val:global_addr_version *= *(\\d+) *$");
	int addrVersion = -1;
	int currVersion = (int)addresses->GetValue("global", "addr_version");

	if (std::regex_search(addrContent, m, expr)) {
		addrVersion = std::stoi(m[1].str());
	}

	if (addrVersion != -1 && (addrVersion > currVersion)) {

		std::ofstream addrFile(GAME_ADDRESSES_FILE);
		if (addrFile.fail()) {
			DEBUG_LOG("Updater: Failed to write addresses file.\n");
		}
		else {
			addrFile << addrContent;
			addrFile.close();
			DEBUG_LOG("Updater: Updated game addresses.\n");
		}

		return true;
	}

	return false;
}


void NavigationMenu::CheckForUpdates(bool programUpdateOnly)
{
	DEBUG_LOG("::CheckForUpdates(%d)\n", programUpdateOnly);
	bool updatedAnything = false;

	if (!programUpdateOnly) {

		// Check for addresses update first
		if (UpdateAddresses(&m_updateStatus.error, m_addresses)) {
			m_updateStatus.addrFile = true;
			updatedAnything = true;
		}
	}
	// Now check for new releases
	if (VerifyProgramUpdate(&m_updateStatus.error, m_addresses)) {
		m_updateStatus.programUpdateAvailable = true;
		updatedAnything = true;
	}

	m_updateStatus.verifying = false;
	m_updateStatus.up_to_date = !updatedAnything;

	DEBUG_LOG("::CheckForUpdates() - END\n");
}


void NavigationMenu::RequestCheckForUpdates(bool programUpdateOnly)
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
	m_updateStatus.thread = std::thread(&NavigationMenu::CheckForUpdates, this, programUpdateOnly);
}


void NavigationMenu::SetAddrFile(GameAddressesFile* addresses)
{
	m_addresses = addresses;
	RequestCheckForUpdates(true);
}