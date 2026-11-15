#include "Http.hpp"
#include "SideMenu.hpp"
#include "helpers.hpp"
#include "Settings.hpp"
#include "constants.h"
#include <nlohmann/json.hpp>
#include <regex>
#include <sstream>

bool DownloadProgramUpdate(s_updateStatus* status, GameAddressesFile*, bool verifyOnly)
{
	try {
		auto release = nlohmann::json::parse(Http::Get(Http::RepositoryApi() + "/releases/latest", 4 << 20));
		auto tag = release.at("tag_name").get<std::string>();
		if (!Helpers::VersionGreater(tag.c_str(), PROGRAM_VERSION)) return false;
		for (const auto& asset : release.at("assets")) {
			if (asset.at("name") != PROGRAM_FILENAME) continue;
			status->tagName = tag;
			status->tagNameSeparatorText = std::format("{} - {}", _("sidemenu.changelog"), tag);
			status->changelog = release["body"].is_string() ? release["body"].get<std::string>() : "";
			// The markdown renderer can't display HTML images: drop <img> tags
			static const std::regex imgTag(R"(<img\b[^>]*>(\s*</img>)?)", std::regex::icase);
			status->changelog = std::regex_replace(status->changelog, imgTag, "");
			if (verifyOnly) return true;
			auto digest = asset.at("digest").get<std::string>();
			auto bytes = Http::Get(asset.at("browser_download_url").get<std::string>(), 256 << 20);
			if (bytes.size() != asset.at("size").get<size_t>() || !bytes.starts_with("MZ") ||
				digest != "sha256:" + Http::Sha256(bytes)) throw std::runtime_error("Invalid release asset");
			Http::InstallDownload(UPDATE_TMP_FILENAME ".exe", bytes);
			return true;
		}
	} catch (const std::exception&) {
		status->error = true;
	}
	return false;
}

static bool UpdateAddresses(bool* error, GameAddressesFile* addresses)
{
	try {
		std::string repo = UPDATE_REPO_URL;
		repo.replace(repo.find("github.com"), sizeof("github.com") - 1, "raw.githubusercontent.com");
		auto content = Http::Get(repo + "/master/TKMovesets2/Resources/game_addresses.ini", 4 << 20);
		std::istringstream lines(content);
		std::string line;
		std::regex version(R"(^\s*val:global_addr_version\s*=\s*(\d+)\s*$)");
		std::smatch match;
		while (std::getline(lines, line)) {
			if (!std::regex_match(line, match, version)) continue;
			if (std::stoll(match[1].str()) <= addresses->GetValue("global", "addr_version")) return false;
			Http::InstallDownload(GAME_ADDRESSES_FILE, content);
			return true;
		}
		*error = true;
	} catch (const std::exception&) {
		*error = true;
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


	m_updateStatus.up_to_date = !updatedAnything && !m_updateStatus.error;
	m_updateStatus.verifying = false;

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

	// The loaded address snapshot is shared with workers. Updated disk data is loaded on restart.

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