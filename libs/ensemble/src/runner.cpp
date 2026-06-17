#include "euxis/ensemble/runner.hpp"

#include <algorithm>

namespace euxis::ensemble {

namespace {

auto build_request(const euxis::security::Finding& f,
                   const EnsembleConfig& cfg) -> VoteRequest {
    VoteRequest req;
    req.rule_id     = f.rule_id;
    req.message     = f.message;
    req.source_path = f.primary_location.path;
    req.start_row    = f.primary_location.start_line;
    req.start_column = f.primary_location.start_column;
    req.severity    = f.severity;
    if (!f.primary_location.snippet.empty()) {
        // SourceLocation.snippet is the already-cropped slice
        // produced by the engine that emitted the Finding;
        // truncate again only if the upstream slice is larger
        // than our configured cap.
        const auto& src = f.primary_location.snippet;
        if (src.size() > cfg.max_snippet_bytes) {
            req.snippet = src.substr(0, cfg.max_snippet_bytes);
        } else {
            req.snippet = src;
        }
    }
    return req;
}

void update_confidence(euxis::security::Finding& f,
                       std::size_t true_positive_count,
                       std::size_t total_votes,
                       const EnsembleConfig& cfg) {
    using C = euxis::security::Confidence;
    if (total_votes == 0) return;  // no verifiers run; leave confidence alone

    const auto quorum_size =
        static_cast<std::size_t>(std::max(1, cfg.quorum));
    const bool reached_quorum = true_positive_count >= quorum_size;
    const bool unanimous      = true_positive_count == total_votes;

    if (reached_quorum && unanimous) {
        f.confidence = C::Verified;
    } else if (reached_quorum) {
        f.confidence = C::Probable;
    } else if (true_positive_count > 0) {
        f.confidence = C::Possible;
    } else {
        // All votes returned false_positive — the runner keeps the
        // finding for human review (it isn't a hard delete) but
        // demotes confidence to Unknown so consumers can filter.
        f.confidence = C::Unknown;
    }
}

} // namespace

auto verify(std::vector<euxis::security::Finding> findings,
            std::span<const VerifierPtr> verifiers,
            const EnsembleConfig& config) -> EnsembleResult {
    EnsembleResult out;
    out.stats.findings_verified = findings.size();
    out.findings.reserve(findings.size());

    const auto quorum_size =
        static_cast<std::size_t>(std::max(1, config.quorum));

    for (auto& f : findings) {
        VoteRequest req = build_request(f, config);

        std::size_t true_positive_count = 0;
        std::size_t total_votes         = 0;
        for (const auto& v : verifiers) {
            if (!v) continue;
            auto outcome = v->vote(req);
            ++out.stats.verifiers_called;
            ++total_votes;
            if (outcome.vote.true_positive) ++true_positive_count;
            f.ensemble_votes.push_back(outcome.vote);
        }

        const bool unanimous_when_required =
            !config.require_unanimous || true_positive_count == total_votes;
        const bool confirmed = total_votes > 0 &&
                               true_positive_count >= quorum_size &&
                               unanimous_when_required;

        if (confirmed) {
            ++out.stats.confirmed;
        } else if (total_votes > 0) {
            ++out.stats.rejected;
        }

        update_confidence(f, true_positive_count, total_votes, config);
        out.findings.push_back(std::move(f));
    }

    return out;
}

} // namespace euxis::ensemble
