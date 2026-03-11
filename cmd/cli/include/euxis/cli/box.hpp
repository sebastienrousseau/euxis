#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "euxis/cli/terminal.hpp"

namespace euxis::cli::terminal {

/// Print text inside a rounded Unicode box (or ASCII fallback).
void print_box(std::string_view title, std::string_view body);

/// Print a table inside a rounded box frame.
void print_boxed_table(const std::vector<std::string>& headers,
                       const std::vector<TableRow>& rows);

} // namespace euxis::cli::terminal
