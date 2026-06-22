#include "euxis/identity/did.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <format>
#include <sstream>

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
    (void) std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &utc);
    return std::string{buf};
}

/// Encode raw bytes to a multibase base64url string with 'z' prefix.
/// Uses libsodium's bin2base64 with sodium_base64_VARIANT_URLSAFE_NO_PADDING.
[[nodiscard]] auto encode_multibase(std::span<const std::byte> data) -> std::string {
    const auto encoded_max_len =
        sodium_base64_encoded_len(data.size(), sodium_base64_VARIANT_URLSAFE_NO_PADDING);
    std::string encoded(encoded_max_len, '\0');
    sodium_bin2base64(
        encoded.data(),
        encoded.size(),
        reinterpret_cast<const unsigned char*>(data.data()),
        data.size(),
        sodium_base64_VARIANT_URLSAFE_NO_PADDING
    );
    // sodium_bin2base64 null-terminates; trim to actual length.
    encoded.resize(std::strlen(encoded.c_str()));
    return "z" + encoded;
}

} // namespace

auto create_did(std::string_view agent_id) -> std::string {
    auto did = std::format("did:euxis:{}", agent_id);
    spdlog::debug("Created DID: {}", did);
    return did;
}

auto create_did_document(
    std::string_view agent_id,
    std::span<const std::byte, 32> public_key,
    std::vector<ServiceEndpoint> services
) -> DIDDocument {
    const auto did = create_did(agent_id);
    const auto now = format_iso8601(std::chrono::system_clock::now());

    VerificationMethod vm{
        .id = std::format("{}#keys-1", did),
        .type = "Ed25519VerificationKey2020",
        .controller = did,
        .public_key_multibase = encode_multibase(public_key),
    };

    DIDDocument doc{
        .id = did,
        .context = "https://www.w3.org/ns/did/v1",
        .verification_methods = {std::move(vm)},
        .authentication = {std::format("{}#keys-1", did)},
        .services = std::move(services),
        .created = now,
        .updated = now,
    };

    spdlog::debug("Created DID document for agent '{}'", agent_id);
    return doc;
}

auto resolve_did(std::string_view did)
    -> std::expected<std::pair<std::string, std::string>, std::string> {
    // DID must start with "did:"
    if (!did.starts_with("did:")) {
        return std::unexpected(std::format("Invalid DID format: must start with 'did:': {}", did));
    }

    const auto without_prefix = did.substr(4);
    const auto colon_pos = without_prefix.find(':');
    if (colon_pos == std::string_view::npos) {
        return std::unexpected(
            std::format("Invalid DID format: missing method-specific id: {}", did));
    }

    auto method = std::string{without_prefix.substr(0, colon_pos)};
    auto id = std::string{without_prefix.substr(colon_pos + 1)};

    if (method.empty()) {
        return std::unexpected(std::format("Invalid DID format: empty method: {}", did));
    }
    if (id.empty()) {
        return std::unexpected(std::format("Invalid DID format: empty id: {}", did));
    }

    spdlog::debug("Resolved DID: method='{}', id='{}'", method, id);
    return std::pair{std::move(method), std::move(id)};
}

auto DIDDocument::to_json() const -> nlohmann::json {
    auto j = nlohmann::json{
        {"@context", context},
        {"id", id},
        {"created", created},
        {"updated", updated},
    };

    auto vm_arr = nlohmann::json::array();
    for (const auto& vm : verification_methods) {
        vm_arr.push_back({
            {"id", vm.id},
            {"type", vm.type},
            {"controller", vm.controller},
            {"publicKeyMultibase", vm.public_key_multibase},
        });
    }
    j["verificationMethod"] = std::move(vm_arr);
    j["authentication"] = authentication;

    auto svc_arr = nlohmann::json::array();
    for (const auto& svc : services) {
        svc_arr.push_back({
            {"id", svc.id},
            {"type", svc.type},
            {"serviceEndpoint", svc.service_endpoint},
        });
    }
    j["service"] = std::move(svc_arr);

    return j;
}

} // namespace euxis::identity
