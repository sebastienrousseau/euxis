/// @file
/// @brief Security errors
#pragma once

#include <stdexcept>
#include <string>

namespace euxis::security {

class PolicyError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

} // namespace euxis::security
