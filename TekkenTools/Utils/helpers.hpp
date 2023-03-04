#pragma once

#include <map>
#include <string>

#include "GameAddresses.h"


namespace Helpers
{
    // Make sure a value is divislbe by 8
    uint64_t align8Bytes(uint64_t value);

    // Make sure a file's cursor is divisble by 8
    void align8Bytes(std::ofstream& file);

    // Gets the current date format in 'hour:minutes day/month/year'
    std::string currentDateTime(uint64_t date);

    // In a (moveset) list, can convert ptr members of every structure in the list into offsets
    void convertPtrsToOffsets(void* listAddr, uint64_t to_substract, uint64_t struct_size, uint64_t amount);
    void convertPtrsToOffsets(void* listAddr, std::map<gameAddr, uint64_t> m, uint64_t struct_size, uint64_t amount);

    // Returns true if a string ends with [suffix]
    bool endsWith(std::string str, std::string suffix);
    //bool endsWith(std::u32string str, std::u32string suffix);

    // Converts filename into displayable name. U32 for UTF8
    std::string getMovesetNameFromFilename(std::string filename);

    // Returns true if one of the header strings is not properly formatted indicating a bad moveset file
    bool isHeaderStringMalformated(const char* string, size_t size);

    // Returns true if file exists
    bool fileExists(const char* name);
}