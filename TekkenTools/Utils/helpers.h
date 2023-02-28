#pragma once

namespace Helpers
{
    // Format a std::string like std::format() would
    template<typename ... Args>
    std::string string_format(const std::string& format, Args ... args);
}