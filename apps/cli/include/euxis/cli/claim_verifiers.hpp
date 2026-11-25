#pragma once
/// @file
/// @brief Claim verification: back agent claims by running the scanned
///        repository's own build and test commands.
///
/// Running those commands executes code from the repository being
/// scanned, so `run_claim_verification` does nothing unless the user
/// opts in (`--run-verifiers`). Never enable it for untrusted code,
/// such as pull requests from forks.

#include <iosfwd>
#include <string>
#include <utility>
#include <vector>

namespace euxis::cli::claims {

struct VerificationResult {
    std::string check_name;
    std::string command;
    bool success{false};
    double duration_ms{0.0};
    int claims_upgraded{0};
    std::string output_summary;
};

/// (name, argv) pairs.
using VerifierList = std::vector<std::pair<std::string, std::vector<std::string>>>;

/// The repo's `.euxis.yaml` `verify:` commands, or commands detected
/// from its manifests (Cargo.toml, package.json, go.mod, Makefile, …).
[[nodiscard]] auto detect_project_verifiers(const std::string& repo_root) -> VerifierList;

/// Run each verifier with a per-command timeout.
[[nodiscard]] auto run_claim_verifiers(const VerifierList& verifiers, int timeout_per_check = 60)
    -> std::vector<VerificationResult>;

/// Evidence pillar ("build", "testing") a verifier backs, or "".
[[nodiscard]] auto verifier_pillar(const std::string& name) -> std::string;

struct ClaimVerification {
    std::vector<VerificationResult> results;
    int claims_upgraded{0};
};

/// Detect and run the verifiers in the current directory when
/// `enabled`; otherwise print a notice to `tui` and run nothing.
[[nodiscard]] auto run_claim_verification(int timeout_s, bool enabled, std::ostream& tui)
    -> ClaimVerification;

}  // namespace euxis::cli::claims
