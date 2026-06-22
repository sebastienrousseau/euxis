#include "euxis/ensemble/response.hpp"

#include <algorithm>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

namespace euxis::ensemble {

namespace {

/// Strip Markdown code-fence wrappers (```json ... ``` and ``` ... ```).
std::string_view strip_code_fence(std::string_view body) {
    constexpr std::string_view fence = "```";
    auto open = body.find(fence);
    if (open == std::string_view::npos) return body;
    auto inner_start = body.find('\n', open + fence.size());
    if (inner_start == std::string_view::npos) return body;
    inner_start += 1;
    auto close = body.find(fence, inner_start);
    if (close == std::string_view::npos) return body;
    return body.substr(inner_start, close - inner_start);
}

/// Locate the first top-level `{ ... }` JSON object in `body`.
/// Returns the substring on success, or empty when no balanced
/// object is found.
std::string_view first_json_object(std::string_view body) {
    auto open = body.find('{');
    if (open == std::string_view::npos) return {};
    std::size_t depth   = 0;
    bool in_string      = false;
    bool escape_next    = false;
    for (std::size_t i = open; i < body.size(); ++i) {
        char c = body[i];
        if (escape_next) { escape_next = false; continue; }
        if (in_string) {
            if (c == '\\')      escape_next = true;
            else if (c == '"')  in_string = false;
            continue;
        }
        if (c == '"')      in_string = true;
        else if (c == '{') ++depth;
        else if (c == '}') {
            if (depth == 0) break;
            --depth;
            if (depth == 0) {
                return body.substr(open, i - open + 1);
            }
        }
    }
    return {};
}

bool coerce_bool(const nlohmann::json& v) {
    if (v.is_boolean()) return v.get<bool>();
    if (v.is_string()) {
        std::string s = v.get<std::string>();
        std::transform(s.begin(), s.end(), s.begin(),
            [](unsigned char c) { return std::tolower(c); });
        return s == "true" || s == "yes" || s == "1";
    }
    if (v.is_number()) return v.get<double>() != 0.0;
    return false;
}

float coerce_confidence(const nlohmann::json& v) {
    if (v.is_number()) {
        double c = v.get<double>();
        if (c < 0.0) c = 0.0;
        if (c > 1.0) c = 1.0;
        return static_cast<float>(c);
    }
    if (v.is_string()) {
        try {
            double c = std::stod(v.get<std::string>());
            if (c < 0.0) c = 0.0;
            if (c > 1.0) c = 1.0;
            return static_cast<float>(c);
        } catch (...) {
            (void)0;  // swallowed: best-effort — fall through to default
        }
    }
    return 0.0F;
}

std::string coerce_rationale(const nlohmann::json& v) {
    std::string out;
    if (v.is_string()) out = v.get<std::string>();
    if (out.size() > 200) out.resize(200);
    return out;
}

} // namespace

auto parse_response(std::string_view body, std::string provider_id)
    -> std::expected<VoteOutcome, ParseError> {

    auto stripped = strip_code_fence(body);
    auto json_str = first_json_object(stripped);
    if (json_str.empty()) {
        return std::unexpected(ParseError{
            .message = "no JSON object in response body",
        });
    }

    nlohmann::json parsed;
    try {
        parsed = nlohmann::json::parse(json_str);
    } catch (const nlohmann::json::parse_error& e) {
        return std::unexpected(ParseError{
            .message = std::string{"JSON parse: "} + e.what(),
        });
    }
    if (!parsed.is_object()) {
        return std::unexpected(ParseError{
            .message = "response is not a JSON object",
        });
    }

    auto tp = parsed.find("true_positive");
    auto cf = parsed.find("confidence");
    auto rt = parsed.find("rationale");
    if (tp == parsed.end()) {
        return std::unexpected(ParseError{
            .message = "missing `true_positive` field",
        });
    }

    VoteOutcome out;
    out.vote.provider      = std::move(provider_id);
    out.vote.true_positive = coerce_bool(*tp);
    out.vote.confidence    = (cf != parsed.end()) ? coerce_confidence(*cf) : 0.0F;
    out.rationale          = (rt != parsed.end()) ? coerce_rationale(*rt) : "";
    return out;
}

} // namespace euxis::ensemble
