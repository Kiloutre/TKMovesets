#pragma once
#include "LocalStorage.hpp"
#include "AnimExtractors/AnimExtractors.hpp"
#include <atomic>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>
#include <vector>

// Extraction menu, Tekken 8 retail and Tekken 7: every checked character's moveset built from the game's own files
// (tkdata.bin; RetailT8Disk / T7Disk), one .tkmvst each. The modal asks for the game's exe (typed, pasted or browsed; filled in
// from the attached game), whose install holds tkdata.bin. Building starts only on its Start button and can be interrupted (between movesets).
class T8DiskExtraction {
public:
    enum class Game { T8Retail, T7 };
private:
    struct Item {
        std::string code, name, error;
        bool selected = false; // built only when checked; none are by default (All / None buttons)
        std::atomic<int> status{AnimExtractionStatus_NotStarted};
        std::atomic<uint32_t> animations{0}, moves{0};
    };
    std::filesystem::path m_tkdata; // empty until the exe path gave a character list
    char m_executable[1024]{};      // UTF-8 path of the targeted game exe
    Game m_game = Game::T8Retail;
    bool m_popup = false, m_started = false;
    std::thread m_thread;
    std::atomic<bool> m_running{false}, m_interrupt{false}, m_refresh{false};
    bool m_categories[6]{true, true, false, false, true, false}; // body, hands, camera: the export default
    std::vector<std::unique_ptr<Item>> m_items;
    std::string m_error;
    void Build(std::filesystem::path tkdata, uint8_t categories);
    void LoadCharacters(); // character list of m_executable's install
public:
    // `gameExecutable`: the attached game's exe (empty when not attached), filled into the modal's path field.
    void Render(Game game, const std::filesystem::path& gameExecutable, bool enabled, LocalStorage& storage);
    ~T8DiskExtraction();
};
