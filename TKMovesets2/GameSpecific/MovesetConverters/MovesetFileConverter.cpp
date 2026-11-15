#include "constants.h"
#include "MovesetFileConverter.hpp"
#include "MovesetFile.hpp"
#include "FileIO.hpp"
#include "Compression.hpp"

bool CanConvertMoveset(uint16_t source, uint16_t target, uint16_t targetVersion)
{
    if (target == GameId_T8) return targetVersion == GameVersions::T8::RETAIL && source == GameId_T7;
    return target == GameId_T7 && (source == GameId_TTT2 || source == GameId_TREV || source == GameId_T6 || source == GameId_T5 ||
        source == GameId_T8);
}

std::filesystem::path ConvertedMovesetPath(const movesetInfo& info, uint16_t target)
{
    if (target != GameId_T7 && target != GameId_T8) throw std::invalid_argument("Unsupported conversion target");
    // The source name stays (prefix included) and gets the target game: T8_Jin Kazama -> "T8_Jin Kazama (T7)".
    std::filesystem::path source(info.filename);
    auto name = source.stem().wstring() + (target == GameId_T8 ? L" (T8)" : L" (T7)");
    auto output = source.parent_path() / (name + L"" MOVESET_FILENAME_EXTENSION);
    for (unsigned counter = 2; std::filesystem::exists(output); ++counter)
        output = source.parent_path() / (name + L" (" + std::to_wstring(counter) + L")" MOVESET_FILENAME_EXTENSION);
    return output;
}
