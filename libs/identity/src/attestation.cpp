#include "euxis/identity/attestation.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <format>
#include <stdexcept>

#include <sodium.h>
#include <spdlog/spdlog.h>

namespace euxis::identity {

namespace {

/// Format a time_point as an ISO-8601 UTC timestamp "YYYY-MM-DDTHH:MM:SSZ".
[[nodiscard]] auto format_iso8601(std::chrono::system_clock::time_point tp) -> std::string {
    const auto tt = std::chrono::system_clock::to_time_t(tp);
    std::tm utc{};
    gmtime_r(&tt, &utc);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &utc);
    return std::string{buf};
}

/// Generate a random hex ID using libsodium.
[[nodiscard]] auto generate_attestation_id() -> std::string {
    std::array<unsigned char, 16> buf{};
    randombytes_buf(buf.data(), buf.size());

    char hex[37];
    std::snprintf(hex, sizeof(hex),
        "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        buf[0], buf[1], buf[2], buf[3],
        buf[4], buf[5],
        buf[6], buf[7],
        buf[8], buf[9],
        buf[10], buf[11], buf[12], buf[13], buf[14], buf[15]);
    return std::string{hex};
}

} // namespace

auto create_attestation(
    std::string_view attester_did,
    std::string_view subject_did,
    std::string_view type,
    double confidence,
    std::string_view evidence
) -> Attestation {
    if (confidence < 0.0 || confidence > 1.0) {
        throw std::invalid_argument(
            std::format("Confidence must be in [0.0, 1.0], got {}", confidence));
    }

    Attestation att{
        .id = generate_attestation_id(),
        .attester_did = std::string{attester_did},
        .subject_did = std::string{subject_did},
        .attestation_type = std::string{type},
        .confidence = confidence,
        .created_at = format_iso8601(std::chrono::system_clock::now()),
        .evidence = std::string{evidence},
    };

    spdlog::debug("Created attestation {} from {} about {}", att.id, attester_did, subject_did);
    return att;
}

auto Attestation::to_json() const -> nlohmann::json {
    return nlohmann::json{
        {"id", id},
        {"attester", attester_did},
        {"subject", subject_did},
        {"type", attestation_type},
        {"confidence", confidence},
        {"createdAt", created_at},
        {"evidence", evidence},
    };
}

} // namespace euxis::identity
