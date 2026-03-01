#pragma once

#include <stdexcept>
#include <string>

namespace euxis::runtime {

class RuntimeValidationError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

} // namespace euxis::runtime
