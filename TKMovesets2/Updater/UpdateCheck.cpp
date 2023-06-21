#include <curlpp/cURLpp.hpp>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/Infos.hpp>
#include <sstream>
#include <regex>
#include <filesystem>
#include <fstream>

#include "SideMenu.hpp"
#include "helpers.hpp"
#include "Settings.hpp"

#include "constants.h"

// -- Private methods -- //


bool DownloadProgramUpdate(s_updateStatus* updateStatus, GameAddressesFile* addresses, bool verify_only)
{
	std::string repoUrl = UPDATE_REPO_URL;
	std::string releasesUrl = repoUrl;
	releasesUrl.replace(releasesUrl.find("github.com"), sizeof("github.com") - 1, "api.github.com/repos");
	releasesUrl += "/releases/latest";

	DEBUG_LOG("Querying '%s'...\n", releasesUrl.c_str());

	// Have to set up user agent or github will deny the request
	std::string releasesContent;
	{
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
			updateStatus->error = true;
			DEBUG_ERR("CURL ERROR");
			return false;
		}
		catch (curlpp::LogicError&) {
			updateStatus->error = true;
			DEBUG_ERR("CURL ERROR");
			return false;
		}
	}


	std::smatch m;
	std::regex urlExpr("browser_download_url\" *: *\"([^\"]+)\"");
	std::regex tagExpr("tag_name\" *: *\"([^\"]+)\"");
	std::regex bodyExpr("body\" *: *\"([^\"]+)\"");
	std::string downloadUrl;
	std::string tagName;
	std::string changelog;

	if (std::regex_search(releasesContent, m, urlExpr)) {
		downloadUrl = m[1].str();
	}
	if (std::regex_search(releasesContent, m, tagExpr)) {
		tagName = m[1].str();
	}
	if (std::regex_search(releasesContent, m, bodyExpr)) {
		changelog = m[1].str();
	}

	DEBUG_LOG("Remote URL: [%s]\n", downloadUrl.c_str());
	DEBUG_LOG("Remote tagname: [%s]\n", tagName.c_str());

	if (downloadUrl.empty() || tagName.empty()) {
		return false;
	}

	updateStatus->tagName = tagName;
	updateStatus->tagNameSeparatorText = std::format("{} - {}", _("sidemenu.changelog"), tagName);
	{
		std::vector<std::string> changelogLines;
		std::string delim = "\n";

		changelog = std::regex_replace(changelog, std::regex("(\\\\r)?\\\\n"), "\n");

		size_t start = 0;
		size_t end = changelog.find(delim);
		while (end != std::string::npos)
		{
			changelogLines.push_back(changelog.substr(start, end - start));
			start = end + delim.size();
			end = changelog.find(delim, start);
		}
		changelogLines.push_back(changelog.substr(start, end));
		updateStatus->changelog = changelogLines;
	}

	if (!Helpers::VersionGreater(tagName.c_str(), PROGRAM_VERSION)) {
		return false;
	}

	if (!Helpers::endsWith<std::string>(downloadUrl, ".exe")) {
		// todo: Maybe handle .7z & .zip releases
		DEBUG_LOG("Bad release extension (not .exe), can't install.\n");
		return false;
	}

	// In case other extensions are ever handled, fetch the extension dynamically to write the proper file name
	std::string extension = downloadUrl.substr(downloadUrl.find_last_of("."));

	if (verify_only) {
		DEBUG_LOG("New release available.\n");
	} else {
		DEBUG_LOG("New release download...\n");

		// Finally download the file
		{
			bool validFile = true;
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

				// First validation step is filesize
				size_t fileSize = ofs.tellp();
				if (fileSize < UPDATE_MIN_FILESIZE) {
					DEBUG_ERR("File size too small (%llu)", fileSize);
					validFile = false;
				}
			}
			catch (curlpp::LibcurlRuntimeError&) {
				updateStatus->error = true;
				DEBUG_ERR("CURL ERROR");
				return false;
			}
			catch (curlpp::LogicError&) {
				updateStatus->error = true;
				DEBUG_ERR("CURL ERROR");
				return false;
			}

			// Second validation step, after we obtained the .exe, is magic bytes
			std::wstring exe_filename = std::wstring(L"" UPDATE_TMP_FILENAME) + L".exe";

			if (!Helpers::fileExists(exe_filename.c_str())) {
				validFile = false;
				DEBUG_LOG("Update: .exe file does not exist.\n");
			}

			if (validFile) {
				std::ifstream executableFile(exe_filename, std::ios::binary);

				unsigned char buf[2]{ 0 };
				executableFile.read((char*)buf, 2);

				if (buf[0] != 0x4D || buf[1] != 0x5A) {
					DEBUG_LOG("Bad magic bytes (%x %x).\n", buf[0], buf[1]);
					validFile = false;
				}
			}

			if (!validFile) {
				DEBUG_LOG("Invalid file, removing.\n");
				updateStatus->error = true;
				if (Helpers::fileExists(exe_filename.c_str())) {
					std::filesystem::remove(exe_filename);
				}
				return false;
			}

		}
	}

	return true;
}


static bool UpdateAddresses(bool* error, GameAddressesFile* addresses)
{
	std::string addrUrl = UPDATE_REPO_URL;
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
		DEBUG_ERR("CURL ERROR");
		return false;
	}
	catch (curlpp::LogicError&) {
		*error = true;
		DEBUG_ERR("CURL ERROR");
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


void SideMenu::CheckForUpdates(bool firstTime)
{
	DEBUG_LOG("::CheckForUpdates(%d)\n", firstTime);
	bool updatedAnything = false;

	
	if (!firstTime || Settings::Get(SETTING_AUTO_UPDATE_KEY, SETTING_AUTO_UPDATE) == 1) {
		// Now check for new releases
		if (DownloadProgramUpdate(&m_updateStatus, m_addresses, true)) {
			m_updateStatus.programUpdateAvailable = true;
			m_updateFileInvalid = false;
			updatedAnything = true;
		}
	}

	if (!firstTime || Settings::Get(SETTING_AUTO_ADDR_UPDATE_KEY, SETTING_AUTO_ADDR_UPDATE) == 1) {
		// Check for addresses update first
		if (UpdateAddresses(&m_updateStatus.error, m_addresses)) {
			m_updateStatus.addrFile = true;
			updatedAnything = true;
		}
	}


	m_updateStatus.verifying = false;
	m_updateStatus.up_to_date = !updatedAnything;

	DEBUG_LOG("::CheckForUpdates() - END\n");
}


void SideMenu::RequestCheckForUpdates()
{
	if (m_addresses == nullptr || m_updateStatus.verifying) {
		return;
	}
	if (m_updateStatus.verifiedOnce) {
		m_updateStatus.thread.join();
	}

	// Reload addresses before updating, in case the loaded game addresses is not up to date but the local file one is
	m_addresses->Reload();

	m_updateStatus.error = false;
	m_updateStatus.addrFile = false;
	m_updateStatus.up_to_date = false;

	m_updateStatus.verifying = true;
	// Start thread to avoid HTTP thread hanging the display thread
	m_updateStatus.thread = std::thread(&SideMenu::CheckForUpdates, this, !m_updateStatus.verifiedOnce);
	m_updateStatus.verifiedOnce = true;
}


void SideMenu::SetAddrFile(GameAddressesFile* addresses)
{
	m_addresses = addresses;
#ifndef BUILD_TYPE_DEBUG
	// Automatically check for updates in release mode
	RequestCheckForUpdates();
#endif
}

void SideMenu::CleanupThread()
{
	if (m_updateStatus.verifiedOnce) {
		m_updateStatus.thread.join();
		m_updateStatus.verifiedOnce = false;
	}
}