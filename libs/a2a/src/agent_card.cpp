#include "euxis/a2a/agent_card.hpp"

#include <spdlog/spdlog.h>
#include <cstring>

namespace euxis::a2a {

// ---------------------------------------------------------------------------
// AgentCard::to_json  (A2A v0.2 camelCase)
// ---------------------------------------------------------------------------
nlohmann::json AgentCard::to_json() const {
    nlohmann::json j;
    j["name"] = name;
    j["description"] = description;
    j["url"] = url;
    j["version"] = version;

    auto caps = nlohmann::json::array();
    for (const auto& cap : capabilities) {
        nlohmann::json c;
        c["name"] = cap.name;
        c["description"] = cap.description;
        if (cap.input_schema.has_value()) {
            c["inputSchema"] = *cap.input_schema;
        }
        if (cap.output_schema.has_value()) {
            c["outputSchema"] = *cap.output_schema;
        }
        caps.push_back(std::move(c));
    }
    j["capabilities"] = std::move(caps);

    if (auth.has_value()) {
        j["auth"] = {
            {"type", auth->type},
            {"description", auth->description}
        };
    }

    if (identity_did.has_value()) {
        j["identityDid"] = *identity_did;
    }

    if (!metadata.is_null()) {
        j["metadata"] = metadata;
    }

    return j;
}

// ---------------------------------------------------------------------------
// AgentCard::from_json
// ---------------------------------------------------------------------------
auto AgentCard::from_json(const nlohmann::json& j) -> AgentCard {
    AgentCard card;
    card.name = j.at("name").get<std::string>();
    card.description = j.at("description").get<std::string>();
    card.url = j.at("url").get<std::string>();
    card.version = j.at("version").get<std::string>();

    if (j.contains("capabilities")) {
        for (const auto& c : j.at("capabilities")) {
            Capability cap;
            cap.name = c.at("name").get<std::string>();
            cap.description = c.at("description").get<std::string>();
            if (c.contains("inputSchema")) {
                cap.input_schema = c.at("inputSchema");
            }
            if (c.contains("outputSchema")) {
                cap.output_schema = c.at("outputSchema");
            }
            card.capabilities.push_back(std::move(cap));
        }
    }

    if (j.contains("auth")) {
        AuthSchema as;
        as.type = j.at("auth").at("type").get<std::string>();
        as.description = j.at("auth").at("description").get<std::string>();
        card.auth = std::move(as);
    }

    if (j.contains("identityDid")) {
        card.identity_did = j.at("identityDid").get<std::string>();
    }

    if (j.contains("metadata")) {
        card.metadata = j.at("metadata");
    }

    return card;
}

// ---------------------------------------------------------------------------
// High-performance binary serialization
// ---------------------------------------------------------------------------

namespace {
    // Writes string length-prefix + data via raw pointer cursor — no indirection.
    inline void write_str(uint8_t*& p, const std::string& s) {
        auto len = static_cast<uint32_t>(s.size());
        std::memcpy(p, &len, 4);
        std::memcpy(p + 4, s.data(), s.size());
        p += 4 + s.size();
    }

    inline void write_u32(uint8_t*& p, uint32_t v) {
        std::memcpy(p, &v, 4);
        p += 4;
    }

    // Reads length-prefixed string directly into destination — no temporary.
    inline void read_into(const uint8_t*& p, const uint8_t* end, std::string& dest) {
        if (end - p < 4) [[unlikely]]
            throw std::runtime_error("msgpack: truncated string length");
        uint32_t len = 0;
        std::memcpy(&len, p, 4);
        p += 4;
        if (len > static_cast<uint32_t>(end - p)) [[unlikely]]
            throw std::runtime_error("msgpack: string length exceeds buffer");
        dest.assign(reinterpret_cast<const char*>(p), len);
        p += len;
    }
}

auto AgentCard::to_msgpack() const -> std::vector<uint8_t> {
    // Compute exact payload size — single allocation, zero reallocations.
    size_t total = (4 + name.size()) + (4 + description.size())
                 + (4 + url.size()) + (4 + version.size()) + 4;
    for (const auto& cap : capabilities) {
        total += (4 + cap.name.size()) + (4 + cap.description.size());
    }

    std::vector<uint8_t> buf(total);
    auto* p = buf.data();
    write_str(p, name);
    write_str(p, description);
    write_str(p, url);
    write_str(p, version);
    write_u32(p, static_cast<uint32_t>(capabilities.size()));
    for (const auto& cap : capabilities) {
        write_str(p, cap.name);
        write_str(p, cap.description);
    }
    return buf;
}

auto AgentCard::from_msgpack(const std::vector<uint8_t>& data) -> AgentCard {
    AgentCard card;
    const uint8_t* p = data.data();
    const uint8_t* end = data.data() + data.size();
    read_into(p, end, card.name);
    read_into(p, end, card.description);
    read_into(p, end, card.url);
    read_into(p, end, card.version);

    if (end - p < 4)
        throw std::runtime_error("msgpack: truncated capability count");
    uint32_t num_caps = 0;
    std::memcpy(&num_caps, p, 4);
    p += 4;

    if (num_caps > 10000)
        throw std::runtime_error("msgpack: unreasonable capability count");

    card.capabilities.reserve(num_caps);
    for (uint32_t i = 0; i < num_caps; ++i) {
        auto& cap = card.capabilities.emplace_back();
        read_into(p, end, cap.name);
        read_into(p, end, cap.description);
    }
    return card;
}

// ---------------------------------------------------------------------------
// validate_card
// ---------------------------------------------------------------------------
auto validate_card(const AgentCard& card) -> std::expected<void, std::string> {
    if (card.name.empty()) {
        spdlog::warn("A2A agent card validation failed: name is empty");
        return std::unexpected(std::string{"agent card name must not be empty"});
    }
    if (card.url.empty()) {
        spdlog::warn("A2A agent card validation failed: url is empty");
        return std::unexpected(std::string{"agent card url must not be empty"});
    }
    if (card.capabilities.empty()) {
        spdlog::warn("A2A agent card validation failed: no capabilities");
        return std::unexpected(std::string{"agent card must have at least one capability"});
    }
    return {};
}

} // namespace euxis::a2a
