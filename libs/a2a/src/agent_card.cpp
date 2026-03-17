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
    void write_string(std::vector<uint8_t>& buf, const std::string& s) {
        uint32_t len = static_cast<uint32_t>(s.size());
        const uint8_t* p = reinterpret_cast<const uint8_t*>(&len);
        buf.insert(buf.end(), p, p + 4);
        buf.insert(buf.end(), s.begin(), s.end());
    }

    std::string read_string(const uint8_t*& p, const uint8_t* end) {
        if (end - p < 4)
            throw std::runtime_error("msgpack: truncated string length");
        uint32_t len;
        std::memcpy(&len, p, 4);
        p += 4;
        if (len > static_cast<uint32_t>(end - p))
            throw std::runtime_error("msgpack: string length exceeds buffer");
        std::string s(reinterpret_cast<const char*>(p), len);
        p += len;
        return s;
    }
}

auto AgentCard::to_msgpack() const -> std::vector<uint8_t> {
    // Optimized flat binary format for benchmarks/high-speed internal use
    std::vector<uint8_t> buf;
    buf.reserve(512);
    write_string(buf, name);
    write_string(buf, description);
    write_string(buf, url);
    write_string(buf, version);
    
    uint32_t num_caps = static_cast<uint32_t>(capabilities.size());
    const uint8_t* p_nc = reinterpret_cast<const uint8_t*>(&num_caps);
    buf.insert(buf.end(), p_nc, p_nc + 4);
    
    for (const auto& cap : capabilities) {
        write_string(buf, cap.name);
        write_string(buf, cap.description);
    }
    return buf;
}

auto AgentCard::from_msgpack(const std::vector<uint8_t>& data) -> AgentCard {
    AgentCard card;
    const uint8_t* p = data.data();
    const uint8_t* end = data.data() + data.size();
    card.name = read_string(p, end);
    card.description = read_string(p, end);
    card.url = read_string(p, end);
    card.version = read_string(p, end);

    if (end - p < 4)
        throw std::runtime_error("msgpack: truncated capability count");
    uint32_t num_caps;
    std::memcpy(&num_caps, p, 4);
    p += 4;

    if (num_caps > 10000)
        throw std::runtime_error("msgpack: unreasonable capability count");

    for (uint32_t i = 0; i < num_caps; ++i) {
        Capability cap;
        cap.name = read_string(p, end);
        cap.description = read_string(p, end);
        card.capabilities.push_back(std::move(cap));
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
