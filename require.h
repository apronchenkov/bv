#pragma once

#include <boost/format.hpp>
#include <stdexcept>
#include <string>


inline void require(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

inline void require(bool condition, const std::string& message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

inline void require(bool condition, const boost::format& message)
{
    if (!condition) {
        throw std::runtime_error(message.str());
    }
}
