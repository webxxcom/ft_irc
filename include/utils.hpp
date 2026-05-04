#pragma once

#include <string>

namespace irc
{
    inline std::string lowercaseStr(std::string const& str)
    {
        std::string cpy;

        for(size_t i = 0; i < str.size(); ++i)
            cpy.push_back((char)std::tolower(str[i]));
        return cpy;
    }
}