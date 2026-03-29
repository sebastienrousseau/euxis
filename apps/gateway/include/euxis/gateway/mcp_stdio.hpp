#pragma once

#include "euxis/gateway/mcp.hpp"

#include <iostream>

namespace euxis::gateway {

/// MCP server over stdio with Content-Length framing.
class McpStdioServer {
public:
    explicit McpStdioServer(McpHost& host);

    /// Run the stdio server loop. Blocks until EOF on stdin.
    void run();

    /// Process a single frame from input stream. Returns false on EOF.
    bool process_one(std::istream& in, std::ostream& out);

    /// Write a JSON-RPC response with Content-Length framing.
    static void write_frame(std::ostream& out, const nlohmann::json& message);

    /// Read a Content-Length framed message from input.
    static auto read_frame(std::istream& in) -> std::optional<nlohmann::json>;

private:
    McpHost& host_;
};

} // namespace euxis::gateway
