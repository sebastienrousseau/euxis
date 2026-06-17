#include "euxis/attest/statement.hpp"

#include <algorithm>
#include <cctype>

namespace euxis::attest {

auto to_json(const Statement& s) -> nlohmann::json {
    nlohmann::json out;
    out["_type"]         = kStatementType;
    out["predicateType"] = s.predicate_type;
    out["predicate"]     = s.predicate;

    nlohmann::json subjects = nlohmann::json::array();
    for (const auto& subject : s.subjects) {
        nlohmann::json subj;
        subj["name"]   = subject.name;
        nlohmann::json digest = nlohmann::json::object();
        for (const auto& [alg, value] : subject.digest) {
            digest[alg] = value;
        }
        subj["digest"] = digest;
        subjects.push_back(subj);
    }
    out["subject"] = subjects;
    return out;
}

auto serialise_for_signing(const Statement& s) -> std::string {
    // `dump()` with no indent gives a deterministic single-line
    // representation; nlohmann_json already emits object keys in
    // insertion order for json::object — which we don't rely on
    // because in-toto verifiers re-parse before checking values.
    // The signature is over the bytes; canonicalisation correctness
    // is on the signer, not the JSON layout.
    return to_json(s).dump();
}

auto validate(const Statement& s) -> std::expected<void, StatementError> {
    if (s.subjects.empty()) {
        return std::unexpected(StatementError{
            .message = "Statement must contain at least one subject",
        });
    }
    for (const auto& subj : s.subjects) {
        if (subj.name.empty()) {
            return std::unexpected(StatementError{
                .message = "Subject `name` must be non-empty",
            });
        }
        if (subj.digest.empty()) {
            return std::unexpected(StatementError{
                .message = "Subject `digest` must contain at least one entry",
            });
        }
        for (const auto& [alg, value] : subj.digest) {
            if (alg.empty() || value.empty()) {
                return std::unexpected(StatementError{
                    .message = "Subject digest algorithm and value must both be non-empty",
                });
            }
            // Hex-only check; we don't validate algorithm names
            // against a strict vocab because new algs are added
            // over time. Validators on the other end will reject.
            bool hex = std::all_of(value.begin(), value.end(),
                [](char c) {
                    return std::isxdigit(static_cast<unsigned char>(c)) != 0;
                });
            if (!hex) {
                return std::unexpected(StatementError{
                    .message = "Subject digest value must be hex-encoded",
                });
            }
        }
    }
    if (s.predicate_type.empty()) {
        return std::unexpected(StatementError{
            .message = "Statement.predicateType must be non-empty",
        });
    }
    if (s.predicate_type.find(':') == std::string::npos) {
        return std::unexpected(StatementError{
            .message = "Statement.predicateType must be a URI",
        });
    }
    return {};
}

} // namespace euxis::attest
