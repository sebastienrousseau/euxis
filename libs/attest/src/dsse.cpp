#include "euxis/attest/dsse.hpp"

#include <array>
#include <cstdint>
#include <string>

namespace euxis::attest {

namespace {

constexpr std::string_view kBase64Alphabet =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// noexcept: only std::array value-init + integer ops, no allocation, no throws.
// Required so static-storage initialisation cannot throw an uncatchable
// exception (clang-tidy bugprone-throwing-static-initialization).
constexpr auto base64_index_table() noexcept -> std::array<int, 256> {
    std::array<int, 256> table{};
    for (auto& v : table) v = -1;
    for (std::size_t i = 0; i < kBase64Alphabet.size(); ++i) {
        table[static_cast<unsigned char>(kBase64Alphabet[i])] = static_cast<int>(i);
    }
    return table;
}

constexpr auto kBase64Lookup = base64_index_table();

} // namespace

auto pae(std::string_view payload_type,
         std::span<const std::byte> payload) -> std::string {
    constexpr std::string_view kVersion = "DSSEv1";
    std::string out;
    out.reserve(kVersion.size() + payload_type.size() + payload.size() + 32);
    out.append(kVersion);
    out.push_back(' ');
    out.append(std::to_string(payload_type.size()));
    out.push_back(' ');
    out.append(payload_type);
    out.push_back(' ');
    out.append(std::to_string(payload.size()));
    out.push_back(' ');
    out.append(reinterpret_cast<const char*>(payload.data()), payload.size());
    return out;
}

auto pae(std::string_view payload_type, std::string_view payload) -> std::string {
    return pae(payload_type, std::span<const std::byte>(
        reinterpret_cast<const std::byte*>(payload.data()), payload.size()));
}

auto base64_encode(std::span<const std::byte> data) -> std::string {
    if (data.empty()) return {};
    std::string out;
    out.reserve(((data.size() + 2) / 3) * 4);

    auto byte_at = [&](std::size_t i) -> std::uint32_t {
        return static_cast<std::uint8_t>(data[i]);
    };

    std::size_t i = 0;
    for (; i + 3 <= data.size(); i += 3) {
        std::uint32_t v = (byte_at(i) << 16) | (byte_at(i + 1) << 8) | byte_at(i + 2);
        out.push_back(kBase64Alphabet[(v >> 18) & 0x3F]);
        out.push_back(kBase64Alphabet[(v >> 12) & 0x3F]);
        out.push_back(kBase64Alphabet[(v >>  6) & 0x3F]);
        out.push_back(kBase64Alphabet[ v        & 0x3F]);
    }

    if (i < data.size()) {
        std::uint32_t v = byte_at(i) << 16;
        if (i + 1 < data.size()) v |= byte_at(i + 1) << 8;
        out.push_back(kBase64Alphabet[(v >> 18) & 0x3F]);
        out.push_back(kBase64Alphabet[(v >> 12) & 0x3F]);
        if (i + 1 < data.size()) {
            out.push_back(kBase64Alphabet[(v >> 6) & 0x3F]);
            out.push_back('=');
        } else {
            out.push_back('=');
            out.push_back('=');
        }
    }
    return out;
}

auto base64_encode(std::string_view s) -> std::string {
    return base64_encode(std::span<const std::byte>(
        reinterpret_cast<const std::byte*>(s.data()), s.size()));
}

auto base64_decode(std::string_view encoded)
    -> std::expected<std::vector<std::byte>, DsseError> {
    std::vector<std::byte> out;
    out.reserve((encoded.size() / 4) * 3);

    std::uint32_t buffer = 0;
    int bits = 0;
    for (char c : encoded) {
        if (c == '=' || c == '\n' || c == '\r' || c == ' ') break;
        int v = kBase64Lookup[static_cast<unsigned char>(c)];
        if (v < 0) {
            return std::unexpected(DsseError{
                .message = "base64: invalid character",
            });
        }
        buffer = (buffer << 6) | static_cast<std::uint32_t>(v);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<std::byte>((buffer >> bits) & 0xFF));
        }
    }
    return out;
}

auto to_json(const Envelope& env) -> nlohmann::json {
    nlohmann::json out;
    out["payloadType"] = env.payload_type;
    out["payload"]     = env.payload;
    nlohmann::json sigs = nlohmann::json::array();
    for (const auto& s : env.signatures) {
        sigs.push_back({
            {"keyid", s.keyid},
            {"sig",   s.sig},
        });
    }
    out["signatures"] = sigs;
    return out;
}

auto from_json(const nlohmann::json& j) -> std::expected<Envelope, DsseError> {
    if (!j.is_object()) {
        return std::unexpected(DsseError{.message = "DSSE: not an object"});
    }
    Envelope env;
    auto pt = j.find("payloadType");
    if (pt == j.end() || !pt->is_string()) {
        return std::unexpected(DsseError{.message = "DSSE: payloadType missing or not a string"});
    }
    env.payload_type = pt->get<std::string>();

    auto pl = j.find("payload");
    if (pl == j.end() || !pl->is_string()) {
        return std::unexpected(DsseError{.message = "DSSE: payload missing or not a string"});
    }
    env.payload = pl->get<std::string>();

    auto sigs = j.find("signatures");
    if (sigs != j.end() && sigs->is_array()) {
        for (const auto& s : *sigs) {
            if (!s.is_object()) continue;
            Signature sig;
            if (auto k = s.find("keyid"); k != s.end() && k->is_string()) {
                sig.keyid = k->get<std::string>();
            }
            if (auto v = s.find("sig"); v != s.end() && v->is_string()) {
                sig.sig = v->get<std::string>();
            }
            env.signatures.push_back(std::move(sig));
        }
    }
    return env;
}

} // namespace euxis::attest
