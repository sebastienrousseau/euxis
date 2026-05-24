/// @file
/// @brief Minimal end-to-end example: build an AgentCard, drive an
///        A2AServerHandler through the full A2A v0.2 task lifecycle.
///
/// What this demonstrates:
///   1. Constructing an AgentCard with two capabilities.
///   2. Validating the card with std::expected (C++23 error handling).
///   3. Instantiating an A2AServerHandler.
///   4. Dispatching JSON-RPC requests for agent/card, capabilities/list,
///      task/create, task/get, and task/cancel.
///
/// Build: configure with -DEUXIS_BUILD_EXAMPLES=ON
/// Run:   ./cmake-build/docs/examples/cpp/a2a_minimal_server/euxis_example_a2a_minimal_server

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>
#include <sodium.h>

#include "euxis/a2a/agent_card.hpp"
#include "euxis/a2a/server.hpp"

namespace {

auto build_card() -> euxis::a2a::AgentCard {
    euxis::a2a::AgentCard card;
    card.name = "minimal-example-agent";
    card.description = "Smallest agent that exercises the A2A v0.2 surface.";
    card.url = "http://localhost:9090";
    card.version = "0.2.0";

    euxis::a2a::Capability echo;
    echo.name = "echo";
    echo.description = "Returns the input string unchanged.";
    card.capabilities.push_back(std::move(echo));

    euxis::a2a::Capability uppercase;
    uppercase.name = "uppercase";
    uppercase.description = "Returns the input string in ALL CAPS.";
    card.capabilities.push_back(std::move(uppercase));

    return card;
}

void print_step(std::string_view label, const nlohmann::json& payload) {
    std::cout << "\n[" << label << "]\n"
              << payload.dump(2) << '\n';
}

} // namespace

auto main() -> int {
    // libsodium is required by AgentCard binary serialisation paths.
    if (sodium_init() < 0) {
        std::cerr << "sodium_init() failed\n";
        return EXIT_FAILURE;
    }

    auto card = build_card();

    if (const auto ok = euxis::a2a::validate_card(card); !ok) {
        std::cerr << "AgentCard validation failed: " << ok.error() << '\n';
        return EXIT_FAILURE;
    }

    euxis::a2a::A2AServerHandler handler{std::move(card)};

    // 1. Discovery
    print_step("agent/card",
               handler.handle_request("agent/card", {}));

    print_step("capabilities/list",
               handler.handle_request("capabilities/list", {}));

    // 2. Task lifecycle: create -> get -> cancel -> get
    auto created = handler.handle_request(
        "task/create",
        nlohmann::json{{"message", "Demonstrate the lifecycle"}});
    print_step("task/create", created);

    if (!created.contains("result")) {
        std::cerr << "task/create did not return a result\n";
        return EXIT_FAILURE;
    }
    const auto task_id = created["result"]["id"].get<std::string>();

    print_step("task/get (pending)",
               handler.handle_request("task/get", {{"id", task_id}}));

    print_step("task/cancel",
               handler.handle_request("task/cancel", {{"id", task_id}}));

    print_step("task/get (cancelled)",
               handler.handle_request("task/get", {{"id", task_id}}));

    std::cout << "\nExample completed successfully.\n";
    return EXIT_SUCCESS;
}
