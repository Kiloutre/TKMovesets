#pragma once
#include "GameAddressesFile.hpp"
#include "AddressScanner/AddressUpdate.hpp"

#include <atomic>
#include <filesystem>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

// Tools > Addresses: finds the game's addresses in its executable FILE (byte patterns from game_addresses.ini,
// ExeSignatures) and writes them to game_addresses.ini and to the addresses this program has loaded.
// Nothing is read from or written to a running game. Tekken 8 (retail) only for now.
class Submenu_Addresses {
    struct Target {
        const char* name;     // shown in the game list
        const char* gameKey;  // game_addresses.ini prefix
    };
    std::vector<Target> m_targets;
    int m_target = 0;
    char m_executable[1024]{};
    bool m_executableDetected = false;

    std::thread m_thread;
    std::atomic<bool> m_running{false};
    std::mutex m_mutex;
    std::optional<ExeSignatures::ScanReport> m_pending; // set by the scan thread, applied on the UI thread
    std::string m_pendingError;
    bool m_hasPendingError = false;

    std::vector<ExeSignatures::UpdateRow> m_rows;
    std::string m_status, m_error;
    uint32_t m_scannedTimestamp = 0;

    void Start(const GameAddressesFile& addresses);
    void Apply(GameAddressesFile& addresses, const ExeSignatures::ScanReport& report);
    void RenderResults();
public:
    Submenu_Addresses();
    ~Submenu_Addresses();
    // `workersBusy`: an extraction or import is running, the update waits for it to finish.
    void Render(GameAddressesFile& addresses, bool workersBusy);
};
