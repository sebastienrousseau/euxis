/// @file
/// @brief Internal certification functions exposed for unit testing.
#pragma once

#include "euxis/cli/certification.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace euxis::cli::certification::detail {

namespace fs = std::filesystem;

// Utility helpers
auto check_file(const fs::path& root, const std::string& path) -> bool;
auto full_path(const fs::path& root, const std::string& path) -> std::string;
auto read_file_content(const fs::path& path) -> std::string;

// Gates
auto gate_commit_signing(const fs::path& target, int window,
                         const std::string& since_ref, bool strict) -> GateResult;
auto gate_unit_test_health(const fs::path& target) -> GateResult;
auto gate_build_integrity(const fs::path& target) -> GateResult;
auto gate_docs_accuracy(const fs::path& target,
                        const std::vector<std::string>& known_commands) -> GateResult;
auto gate_security_critical(const fs::path& target) -> GateResult;

// Quality risk
auto analyze_quality_risk(const fs::path& target) -> QualityRisk;

// Evidence & domain evaluation
auto collect_evidence(const fs::path& target,
                      const std::vector<GateResult>& gates,
                      const QualityRisk& qr) -> std::vector<Evidence>;
auto evaluate_domains(std::vector<DomainDefinition>& definitions,
                      const std::vector<Evidence>& evidence) -> std::vector<DomainResult>;

// Finalization
void finalize_result(RunResult& result);

} // namespace euxis::cli::certification::detail
