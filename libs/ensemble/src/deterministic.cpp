#include "euxis/ensemble/deterministic.hpp"

#include <utility>

namespace euxis::ensemble {

DeterministicVerifier::DeterministicVerifier(
    std::string provider_id,
    DeterministicPredicate predicate,
    float confidence_when_true,
    float confidence_when_false,
    std::string rationale_when_true,
    std::string rationale_when_false)
    : provider_id_(std::move(provider_id)),
      predicate_(std::move(predicate)),
      confidence_when_true_(confidence_when_true),
      confidence_when_false_(confidence_when_false),
      rationale_when_true_(std::move(rationale_when_true)),
      rationale_when_false_(std::move(rationale_when_false)) {}

auto DeterministicVerifier::provider_id() const -> std::string {
    return provider_id_;
}

auto DeterministicVerifier::vote(const VoteRequest& req) -> VoteOutcome {
    VoteOutcome out;
    out.vote.provider = provider_id_;
    bool fired = predicate_ ? predicate_(req) : true;
    out.vote.true_positive = fired;
    out.vote.confidence    = fired ? confidence_when_true_ : confidence_when_false_;
    out.rationale          = fired ? rationale_when_true_  : rationale_when_false_;
    return out;
}

} // namespace euxis::ensemble
