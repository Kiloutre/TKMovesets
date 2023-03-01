#pragma once

#include <map>
#include <string>

#include "GameAddresses.h"


namespace Helpers
{
    // Gets the current date format in 'hour:minutes day/month/year'
    std::string currentDateTime();

    // In a (moveset) list, can convert ptr members of every structure in the list into offsets
    void ConvertPtrsToOffsets(void* listAddr, uint64_t to_substract, uint64_t struct_size, uint64_t amount);
    void ConvertPtrsToOffsets(void* listAddr, std::map<gameAddr, uint64_t> m, uint64_t struct_size, uint64_t amount);

    // Returns true if a string ends with [suffix]
    bool endsWith(std::string_view str, std::string_view suffix);
    bool endsWith(std::u32string_view str, std::u32string_view suffix);

    // Converts filename into displayable name. U32 for UTF8
    std::string getMovesetNameFromFilename(std::string filename);
}