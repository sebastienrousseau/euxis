#include "euxis/attest/bundle.hpp"

namespace euxis::attest {

namespace {

auto tlog_to_json(const TlogEntry& t) -> nlohmann::json {
    nlohmann::json out;
    out["logIndex"]        = t.log_index;
    out["integratedTime"]  = t.integrated_time_unix;
    if (!t.log_id.empty())                out["logID"]            = t.log_id;
    if (!t.kind_version_kind.empty()) {
        out["kindVersion"]["kind"]    = t.kind_version_kind;
        out["kindVersion"]["version"] = t.kind_version_version;
    }
    if (!t.inclusion_proof.is_null() && !t.inclusion_proof.empty()) {
        out["inclusionProof"] = t.inclusion_proof;
    }
    return out;
}

auto tlog_from_json(const nlohmann::json& j)
    -> std::expected<TlogEntry, BundleError> {
    if (!j.is_object()) {
        return std::unexpected(BundleError{.message = "tlog entry not an object"});
    }
    TlogEntry t;
    t.log_index            = j.value("logIndex",       std::int64_t{0});
    t.integrated_time_unix = j.value("integratedTime", std::int64_t{0});
    t.log_id               = j.value("logID",          std::string{});
    if (auto kv = j.find("kindVersion"); kv != j.end() && kv->is_object()) {
        t.kind_version_kind    = kv->value("kind",    std::string{});
        t.kind_version_version = kv->value("version", std::string{});
    }
    if (auto ip = j.find("inclusionProof"); ip != j.end() && ip->is_object()) {
        t.inclusion_proof = *ip;
    }
    return t;
}

} // namespace

auto to_json(const Bundle& b) -> nlohmann::json {
    nlohmann::json out;
    out["mediaType"] = b.media_type;

    nlohmann::json vm;
    nlohmann::json pk;
    pk["hint"] = b.public_key.hint;
    if (!b.public_key.raw_b64.empty()) {
        pk["rawBytes"] = b.public_key.raw_b64;
    }
    vm["publicKey"] = pk;

    if (!b.tlog_entries.empty()) {
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& t : b.tlog_entries) arr.push_back(tlog_to_json(t));
        vm["tlogEntries"] = arr;
    }
    out["verificationMaterial"] = vm;

    out["dsseEnvelope"] = to_json(b.dsse_envelope);
    return out;
}

auto from_json(const nlohmann::json& j)
    -> std::expected<Bundle, BundleError> {
    if (!j.is_object()) {
        return std::unexpected(BundleError{.message = "Bundle: not an object"});
    }
    Bundle b;
    if (auto mt = j.find("mediaType"); mt != j.end() && mt->is_string()) {
        b.media_type = mt->get<std::string>();
    }

    auto vm = j.find("verificationMaterial");
    if (vm == j.end() || !vm->is_object()) {
        return std::unexpected(BundleError{
            .message = "Bundle: missing verificationMaterial",
        });
    }
    if (auto pk = vm->find("publicKey"); pk != vm->end() && pk->is_object()) {
        b.public_key.hint    = pk->value("hint",     std::string{});
        b.public_key.raw_b64 = pk->value("rawBytes", std::string{});
    }
    if (auto tlogs = vm->find("tlogEntries");
        tlogs != vm->end() && tlogs->is_array()) {
        for (const auto& t : *tlogs) {
            auto parsed = tlog_from_json(t);
            if (!parsed) {
                return std::unexpected(parsed.error());
            }
            b.tlog_entries.push_back(std::move(*parsed));
        }
    }

    auto env = j.find("dsseEnvelope");
    if (env == j.end()) {
        return std::unexpected(BundleError{
            .message = "Bundle: missing dsseEnvelope",
        });
    }
    auto parsed_env = euxis::attest::from_json(*env);
    if (!parsed_env) {
        return std::unexpected(BundleError{
            .message = "Bundle.dsseEnvelope: " + parsed_env.error().message,
        });
    }
    b.dsse_envelope = std::move(*parsed_env);
    return b;
}

} // namespace euxis::attest
