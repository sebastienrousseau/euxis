/// @file
/// @brief Bridge from the slopsquatting Guard to the canonical Finding.
///
/// Keeps libs/slopsquatting free of a dependency on libs/sca; the
/// scanner integration lives here in a thin adapter that takes a
/// ScanResult and emits canonical Findings (one per match).
#pragma once

#include <vector>

#include "euxis/security/finding.hpp"
#include "euxis/slopsquatting/guard.hpp"

namespace euxis::sca { struct ScanResult; } // forward decl

namespace euxis::slopsquatting {

/// Run the guard against every entry in a ScanResult. Produces one
/// `euxis::security::Finding` per matched package, tagged with
/// rule_id "euxis/slopsquatting/EUXIS-SLOPSQ-001" and severity
/// `High` (slopsquatting is treated as a supply-chain compromise
/// risk, mapped to OWASP A03:2025 Software Supply Chain Failures).
[[nodiscard]] auto check_scan_result(const Guard& guard,
                                     const euxis::sca::ScanResult& scan)
    -> std::vector<euxis::security::Finding>;

} // namespace euxis::slopsquatting
