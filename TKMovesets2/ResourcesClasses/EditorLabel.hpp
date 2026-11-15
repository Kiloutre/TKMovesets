#pragma once

#include <thread>
#include <atomic>

#include "GameInfo.hpp"
#include "GameAddressesFile.hpp"

class EditorLabel
{
private:
	std::atomic_bool reloadPending = false;
	// Map containing the labels
	std::map<int, std::string> m_labels;
	// Thread that will access internet to fetch labels informations
	std::thread m_updateThread;
	// Data string
	std::string m_dataString;
	// Higher priority Data string (Season 1, season 2, etc...)
	std::string m_minorDataString;
	// Pointer to the address file, used to know which repo to query
	GameAddressesFile* m_addrFile;

	// Loads a file into our labels
	void LoadFile(const std::string& dataString, const char* name);
	// Fetches the resources online
	void DownloadFromWebsite();
public:
	// Becomes true if a CURL error is encountered
	std::atomic_bool errored = false;
	// Is true if the CURL request is still ongoing
	std::atomic_bool ongoingQuery = false;
	// Becomes true if the last query successfully updated the file (a change was found)
	std::atomic_bool updated = false;
	// True if the update thread has been started at least once
	bool startedThread = false;

	EditorLabel(const GameInfo* gameInfo, GameAddressesFile* addrFile);
	~EditorLabel();

	// Reload the dictionary for the files, without clearing the old values already inserted in previous loads
	void Reload();
	// Returns a string for the given requirement/property ID
	const char* GetText(int id);
	// Every loaded label (requirement ids below 0x8000, property ids above), for id pickers
	const std::map<int, std::string>& Labels() const { return m_labels; }
	// Request for the downloading of editor labels
	void UpdateFromWebsite();
};