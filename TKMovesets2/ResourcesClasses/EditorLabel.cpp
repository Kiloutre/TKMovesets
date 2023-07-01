#include <fstream>
#include <format>
#include <sstream>
#include <curlpp/cURLpp.hpp>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/Infos.hpp>
#include <regex>

#include "Games.hpp"
#include "EditorLabel.hpp"
#include "Helpers.hpp"
#include "Settings.hpp"

#include "constants.h"

// -- Helpers -- //


static bool downloadFile(const char* url, const char* filepath)
{
	// Ensure the INTERFACE_DATA_DIR folder exists
	CreateDirectoryA(INTERFACE_DATA_DIR, nullptr);

	DEBUG_LOG("\tdownloadFile('%s', '%s')\n", url, filepath);

	std::list<std::string> headers;
	headers.push_back("Host: raw.githubusercontent.com");
	headers.push_back("Accept: */*");
	headers.push_back("User-Agent: curl/7.83.1");

	curlpp::Cleanup myCleanup;
	curlpp::Easy myRequest;
	myRequest.setOpt(new curlpp::options::FollowLocation(true));
	myRequest.setOpt(new curlpp::options::Url(url));
	myRequest.setOpt(new curlpp::options::HttpHeader(headers));
	myRequest.setOpt(new curlpp::options::Timeout(HTTP_REQUEST_TIMEOUT));

	try {
		std::ofstream ofs(filepath, std::ios::out | std::ios::trunc | std::ios::binary);
		curlpp::options::WriteStream ws(&ofs);
		myRequest.setOpt(ws);
		myRequest.perform();
	}
	catch (curlpp::LibcurlRuntimeError&) {
		DEBUG_ERR("CURL ERROR");
		return false;
	}
	catch (curlpp::LogicError&) {
		DEBUG_ERR("CURL ERROR");
		return false;
	}

	return true;
}


// -- Private methods -- //


void EditorLabel::LoadFile(const std::string& dataString, const char* name)
{
	std::string filename;
	filename = std::format("{}/editor_{}_{}.txt", INTERFACE_DATA_DIR, dataString, name);


    if (!Helpers::fileExists(filename.c_str())) {
        return;
    }


	std::ifstream infile;
	std::string line;

	infile.open(filename.c_str());

	while (std::getline(infile, line))
	{
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
			// We remove everything in the line before the comment
			line = line.substr(0, commentStart);
		}

		std::string keyStr = line.substr(idStart, line.find_first_of(" ,", idStart));
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
	DEBUG_LOG("EditorLabel::DownloadFromWebsite()\n");
	std::string repoUrl = UPDATE_REPO_URL;

	// Get url of TKInterface folder
	std::string interfaceResourcesUrl = repoUrl;
	interfaceResourcesUrl.replace(interfaceResourcesUrl.find("github.com"), sizeof("github.com") - 1, "api.github.com/repos");
	interfaceResourcesUrl += "/contents/TKMovesets2/Resources/TKMInterface";

	DEBUG_LOG("EditorLabel::DownloadFromWebsite() - Querying '%s'\n", interfaceResourcesUrl.c_str());

	// Prepare the HTTP request
	std::list<std::string> headers;
	headers.push_back("Host: api.github.com");
	headers.push_back("Accept: */*");
	headers.push_back("User-Agent: curl/7.83.1");

	curlpp::Cleanup myCleanup;
	curlpp::Easy myRequest;
	myRequest.setOpt(new curlpp::options::Url(interfaceResourcesUrl));
	myRequest.setOpt(new curlpp::options::HttpHeader(headers));
	myRequest.setOpt(new curlpp::options::Timeout(HTTP_REQUEST_TIMEOUT));
	
	// Send the request, store to fileList
	std::string fileList;
	{
		std::stringstream os;
		try {
			curlpp::options::WriteStream ws(&os);
			myRequest.setOpt(ws);
			myRequest.perform();
			fileList = os.str();
		}
		catch (curlpp::LibcurlRuntimeError&) {
			errored = true;
			ongoingQuery = false;
			DEBUG_ERR("CURL ERROR");
			return;
		}
		catch (curlpp::LogicError&) {
			errored = true;
			ongoingQuery = false;
			DEBUG_ERR("CURL ERROR");
			return;
		}
	}

	// We will try to download any file with these specific prefixes
	std::vector<std::string> prefixToSearchList = {
		"editor_" + m_dataString
	};
	if (!m_minorDataString.empty()) {
		prefixToSearchList.push_back("editor_" + m_minorDataString);
	}

	size_t separatorPos;
	while (true)
	{
		separatorPos = fileList.find("},");
		std::string currentEntry = fileList.substr(0, separatorPos);

		std::regex nameExpr("name\" *: *\"([^\"]+)\"");
		std::string name;

		std::smatch m;
		if (std::regex_search(currentEntry, m, nameExpr)) {
			name = m[1].str();
			DEBUG_LOG("\tFound file '%s'\n", name.c_str());
		}

		for (auto& prefixToSearch : prefixToSearchList)
		{
			if (!name.empty() && Helpers::startsWith<std::string>(name, prefixToSearch))
			{
				std::string fullFilename = ".\\" INTERFACE_DATA_DIR "\\" + name;
				bool willDownload = false;
				struct _stat buffer;

				if (_stat(fullFilename.c_str(), &buffer) == 0)
				{
					DEBUG_LOG("\t- File already exists\n");
					// File exists. fetch size, this is what we will check in order to see if we should really update
					std::regex sizeExpr("size\" *: *([^,]+),");

					if (std::regex_search(currentEntry, m, sizeExpr))
					{
						int size = atoi(m[1].str().c_str());
						DEBUG_LOG("\t- Current size: %d. Internet size: %d\n", (int)buffer.st_size, size);

						if ((int)buffer.st_size != size) {
							willDownload = true;
						}
					}
					else {
						DEBUG_LOG("\t- Could not find: 'size'\n");
					}
				}
				else {
					// File doesn't exist, download without any check
					DEBUG_LOG("\t- File doesn't exist.\n");
					willDownload = true;
				}


				if (willDownload) {
					// Fetch download link
					std::string download_url;
					std::regex urlExpr("download_url\" *: *\"([^\"]+)\"");

					if (std::regex_search(currentEntry, m, urlExpr)) {
						download_url = m[1].str();
						if (!downloadFile(download_url.c_str(), fullFilename.c_str())) {
							errored = true;
						} else {
							updated = true;
						}
					}
					else {
						DEBUG_LOG("\t- Error: could not find download_url.\n");
					}
				}
				else {
					DEBUG_LOG("\t- Not downloading.\n");
				}

				break;
			}
		}


		if (separatorPos == std::string::npos) break;
		fileList.erase(fileList.begin(), fileList.begin() + separatorPos + 2);
	}

	if (updated) {
		Reload();
	}
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


const char* EditorLabel::GetText(int id) const
{
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

	m_updateThread = std::thread(&EditorLabel::DownloadFromWebsite, this);
	startedThread = true;
	errored = false;
	updated = false;
}
