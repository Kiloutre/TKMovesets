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
    bool endsWith(std::string str, std::string suffix);
    //bool endsWith(std::u32string str, std::u32string suffix);

    // Converts filename into displayable name. U32 for UTF8
    std::string getMovesetNameFromFilename(std::string filename);

    // Returns true if one of the header strings is not properly formatted indicating a bad moveset file
    bool isHeaderStringMalformated(const char* string, size_t size);

    // Returns true if file exists
    bool fileExists(const char* name);

    // printf-like formatting for building stf::string instances
    template<typename ... Args> std::string string_format(const std::string& format, Args ... args);
}