#include "euxis/gateway/mcp_stdio.hpp"

#include <sstream>
#include <optional>

namespace euxis::gateway {

McpStdioServer::McpStdioServer(McpHost& host) : host_(host) {}

void McpStdioServer::run() {
    while (process_one(std::cin, std::cout)) {}
}

bool McpStdioServer::process_one(std::istream& in, std::ostream& out) {
    auto message = read_frame(in);
    if (!message) return false;

    auto response = host_.handle_request(*message);
    if (!response.is_null()) {
        write_frame(out, response);
    }
    return true;
}

void McpStdioServer::write_frame(std::ostream& out, const nlohmann::json& message) {
    std::string body = message.dump();
    out << "Content-Length: " << body.size() << "\r\n\r\n" << body;
    out.flush();
}

auto McpStdioServer::read_frame(std::istream& in) -> std::optional<nlohmann::json> {
    // Read headers until empty line
    std::string line;
    size_t content_length = 0;

    while (std::getline(in, line)) {
        // Remove trailing \r if present
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) break;

        // Parse Content-Length header
        if (line.starts_with("Content-Length: ")) {
            try {
                content_length = std::stoull(line.substr(16));
            } catch (...) {
                return std::nullopt;
            }
        }
    }

    if (in.eof() || content_length == 0) return std::nullopt;

    // Read body
    std::string body(content_length, '\0');
    in.read(body.data(), static_cast<std::streamsize>(content_length));

    if (in.gcount() != static_cast<std::streamsize>(content_length)) {
        return std::nullopt;
    }

    try {
        return nlohmann::json::parse(body);
    } catch (...) {
        return std::nullopt;
    }
}

} // namespace euxis::gateway
