#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "provider.hpp"
#include "session_store.hpp"
#include "tool.hpp"

namespace euxis::runtime {

/// Lifecycle events emitted by AgentSession.
enum class SessionEvent {
    MessageSent,
    ResponseReceived,
    ToolInvoked,
    ModelSwitched,
    SessionSaved,
    SessionLoaded,
    BranchCreated,
};

using SessionEventHandler = std::function<void(SessionEvent, const std::string& detail)>;

/// Unified agent session managing conversation, tools, provider, and persistence.
class AgentSession {
public:
    AgentSession(std::string session_id,
                 std::string agent_id,
                 IProvider* provider,
                 ISessionStore* store);

    /// Send a user message and get the assistant response.
    auto send(const std::string& message) -> ProviderResult;

    /// Register a custom tool.
    void register_tool(ToolDeclaration decl, ToolHandler handler);

    /// Remove a registered tool.
    void remove_tool(const std::string& name);

    /// Set the system prompt for this session.
    void set_system_prompt(const std::string& prompt);

    /// Append additional system context.
    void append_system_context(const std::string& context);

    /// Switch the underlying model.
    auto switch_model(const ModelSpec& spec) -> bool;

    /// Create a conversation branch.
    void branch(const std::string& name);

    /// Compact conversation history, keeping last N messages.
    auto compact(size_t keep_n = 20) -> std::expected<void, std::string>;

    /// Persist the current session state.
    auto save() -> std::expected<void, std::string>;

    /// Load a session from the store.
    auto load(const std::string& branch = "main") -> std::expected<void, std::string>;

    /// Subscribe to lifecycle events.
    void on_event(SessionEventHandler handler);

    [[nodiscard]] auto session_id() const -> const std::string&;
    [[nodiscard]] auto agent_id() const -> const std::string&;
    [[nodiscard]] auto messages() const -> const std::vector<ConversationMessage>&;
    [[nodiscard]] auto current_branch() const -> const std::string&;

private:
    std::string session_id_;
    std::string agent_id_;
    std::string branch_id_{"main"};
    std::string system_prompt_;
    std::string system_context_;
    IProvider* provider_;
    ISessionStore* store_;
    std::unique_ptr<IToolRegistry> tools_;
    std::vector<ConversationMessage> messages_;
    std::vector<SessionEventHandler> event_handlers_;
    ModelSpec current_model_;

    void emit(SessionEvent event, const std::string& detail = {});
    auto build_context() const -> std::string;
};

} // namespace euxis::runtime
