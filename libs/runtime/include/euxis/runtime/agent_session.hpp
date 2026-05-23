#pragma once

#include <expected>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "provider.hpp"
#include "session_store.hpp"

namespace euxis::runtime {

/** @brief Type for tool execution handlers. */
using ToolHandler = std::function<std::expected<nlohmann::json, std::string>(const nlohmann::json&)>;

/** @brief Declaration of a tool's identity and parameters. */
struct ToolDeclaration {
    std::string name;
    std::string description;
    nlohmann::json input_schema;
};

/** @brief Interface for managing and invoking agent tools. */
class IToolRegistry {
public:
    virtual ~IToolRegistry() = default;
    virtual void register_tool(ToolDeclaration decl, ToolHandler handler) = 0;
    virtual auto invoke(const std::string& name, const nlohmann::json& input)
        -> std::expected<nlohmann::json, std::string> = 0;
    virtual auto list_tools() const -> std::vector<ToolDeclaration> = 0;
    virtual void remove_tool(const std::string& name) = 0;
};

/** @brief Internal message type for active conversations. */
struct ConversationMessage : public SessionMessage {
    using SessionMessage::operator=;  // unhide base operator=(const SessionMessage&)

    ConversationMessage() = default;
    ConversationMessage(Role r, std::string c, std::string a, std::string m, std::string t, double d)
        : SessionMessage{r, std::move(c), std::move(a), std::move(m), std::move(t), d, {}} {}

    ConversationMessage(const SessionMessage& other) : SessionMessage(other) {}
    // Compiler-generated operator=(const ConversationMessage&) handles derived-to-derived;
    // the using-declaration above handles base-to-derived assignment without slicing
    // surprises (returns SessionMessage&, so no chained derived-method calls).
};

/** @brief Result of an LLM provider execution. */
struct ProviderResult : public ProviderResponse {};

/** @brief Events emitted during a session's lifecycle. */
enum class SessionEvent {
    MessageSent,
    ResponseReceived,
    ModelSwitched,
    BranchCreated,
    SessionSaved,
    SessionLoaded
};

/** @brief Callback type for session event listeners. */
using SessionEventHandler = std::function<void(SessionEvent, const std::string&)>;

/**
 * @brief Manages a single active agent interaction session.
 */
class AgentSession {
public:
    AgentSession(std::string session_id,
                 std::string agent_id,
                 IProvider* provider,
                 ISessionStore* store = nullptr);

    auto send(const std::string& message) -> ProviderResult;

    void register_tool(ToolDeclaration decl, ToolHandler handler);
    void remove_tool(const std::string& name);

    void set_system_prompt(const std::string& prompt);
    void append_system_context(const std::string& context);

    auto switch_model(const ModelSpec& spec) -> bool;
    void branch(const std::string& name);

    auto compact(size_t keep_n) -> std::expected<void, std::string>;
    auto save() -> std::expected<void, std::string>;
    auto load(const std::string& branch = "main") -> std::expected<void, std::string>;

    void on_event(SessionEventHandler handler);

    auto session_id() const -> const std::string&;
    auto agent_id() const -> const std::string&;
    auto messages() const -> const std::vector<ConversationMessage>&;
    auto current_branch() const -> const std::string&;

private:
    std::string session_id_;
    std::string agent_id_;
    std::string branch_id_{"main"};
    std::string system_prompt_;
    std::string system_context_;
    
    IProvider* provider_;
    ISessionStore* store_;
    std::unique_ptr<IToolRegistry> tools_;
    ModelSpec current_model_;
    
    std::vector<ConversationMessage> messages_;
    std::vector<SessionEventHandler> event_handlers_;

    void emit(SessionEvent event, const std::string& detail = "");
    auto build_context() const -> std::string;
};

} // namespace euxis::runtime
