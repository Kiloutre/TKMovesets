#include <fstream>
#include "EmbeddedInterface.hpp"
#include <sstream>
#include <format>
#include <sstream>
#include "../Updater/Http.hpp"
#include <nlohmann/json.hpp>
#include "Games.hpp"
#include "EditorLabel.hpp"
#include "Helpers.hpp"
#include "Settings.hpp"

#include "constants.h"

// -- Helpers -- //


// -- Private methods -- //


void EditorLabel::LoadFile(const std::string& dataString, const char* name)
{
	// TKMInterface/editor_<game>_<name>.txt on disk if present, else the copy embedded in the exe
	const auto content = EmbeddedInterface::Read(std::format("editor_{}_{}.txt", dataString, name));
	if (!content) {
		return;
	}

	std::istringstream infile(*content);
	std::string line;

	while (std::getline(infile, line))
	{
		// raw bytes (not a text-mode file): drop the CR of CRLF lines
		if (!line.empty() && line.back() == '\r') line.pop_back();
		size_t commentStart = line.find_first_of('#');

		size_t idStart = line.find_first_not_of(' ', 0);
		size_t separator = line.find_first_of(',');

		if (separator == std::string::npos) {
			// Not a 'key = value' entry
			continue;
		}
		if (commentStart != std::string::npos)
		{
			if (commentStart < separator) continue;
            // A hash after the separator is part of the label (e.g. Flag #2).
		}

		if (idStart == std::string::npos || idStart >= separator) continue;
		std::string keyStr = line.substr(idStart, line.find_first_of(" ,", idStart) - idStart);
		std::string value;
		{
			size_t value_start = line.find_first_not_of(" ,", separator);
			if (value_start == std::string::npos) {
				continue;
			}
			value = line.substr(value_start, line.find_last_not_of(" ") + 1 - value_start);
		}

		if (keyStr.length() == 0) {
			continue;
		}

		{
			// May contain \n, so we replace them with the actual '\n' character
			size_t newlinePos = 0;
			while ((newlinePos = value.find("\\n", newlinePos)) != std::string::npos)
			{
				value.replace(newlinePos, 2, "\n");
				newlinePos++;
			}
		}

		int key = (int)strtoll(keyStr.c_str(), nullptr, 0);
		m_labels[key] = value;
	}
}


void EditorLabel::DownloadFromWebsite()
{
	// Every file of Resources/TKMInterface in the repository (labels, parameter tables, fighter names), not only this
	// game's labels. A local file that differs from the repository's is overwritten, edits included.
	try {
		auto files = nlohmann::json::parse(Http::Get(Http::RepositoryApi() + "/contents/TKMovesets2/Resources/TKMInterface", 4 << 20));
		for (const auto& file : files) {
			if (file.value("type", "") != "file") continue;
			auto name = file.at("name").get<std::string>();
			if (name.empty() || std::filesystem::path(name).filename() != name) continue;
			// data files only: never download anything that could be executable
			{
				std::string extension = std::filesystem::path(name).extension().string();
				for (auto& c : extension) c = (char)std::tolower((unsigned char)c);
				if (extension != ".ini" && extension != ".txt" && extension != ".json") continue;
			}
			// the file Read uses, so the download is what gets loaded
			const auto path = EmbeddedInterface::DiskPath(name);
			{
				std::ifstream old(path, std::ios::binary);
				if (old) {
					std::string previous{std::istreambuf_iterator<char>(old), {}};
					// already the repository's version: skip the download
					if (Http::GitBlobSha(previous) == file.at("sha").get<std::string>()) continue;
				}
			}
			auto bytes = Http::Get(file.at("download_url").get<std::string>(), 32 << 20);
			if (bytes.size() != file.at("size").get<size_t>()) throw std::runtime_error("Incomplete interface file");
			std::filesystem::create_directories(path.parent_path());
			Http::InstallDownload(path, bytes);
			updated = true;
		}
	} catch (const std::exception&) {
		errored = true;
	}
	if (updated) reloadPending = true;
	ongoingQuery = false;
}

// -- Public methods -- //

EditorLabel::EditorLabel(const GameInfo* gameInfo, GameAddressesFile* addrFile)
{
	m_addrFile = addrFile;

	m_dataString = gameInfo->dataString;
	if (gameInfo->minorDataString != m_dataString) {
		m_minorDataString = gameInfo->minorDataString;
	}

	Reload();
}


EditorLabel::~EditorLabel()
{
	if (startedThread) {
		m_updateThread.join();
	}
}


void EditorLabel::Reload()
{
	LoadFile(m_dataString, "properties");
	LoadFile(m_dataString, "requirements");

	// Doing these loads here will purposefully overwrite the above files
	if (m_minorDataString != m_dataString && !m_minorDataString.empty()) {
		LoadFile(m_minorDataString, "properties");
		LoadFile(m_minorDataString, "requirements");
	}
}


const char* EditorLabel::GetText(int id)
{
	if (reloadPending.exchange(false)) Reload();
	auto item = m_labels.find(id);
	if (item != m_labels.end()) {
		return item->second.c_str();
	}
	return nullptr;
}


void EditorLabel::UpdateFromWebsite()
{
	if (ongoingQuery) {
		return;
	}
	ongoingQuery = true;

	if (startedThread) {
		m_updateThread.join();
	}

	errored = false;
	updated = false;
	startedThread = true;
	m_updateThread = std::thread(&EditorLabel::DownloadFromWebsite, this);
}
