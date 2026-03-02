#include "euxis/runtime/agent_session.hpp"

#include <chrono>
#include <format>
#include <sstream>

namespace euxis::runtime {

// Forward declaration from tool_registry.cpp
auto make_tool_registry() -> std::unique_ptr<IToolRegistry>;

AgentSession::AgentSession(std::string session_id,
                           std::string agent_id,
                           IProvider* provider,
                           ISessionStore* store)
    : session_id_(std::move(session_id))
    , agent_id_(std::move(agent_id))
    , provider_(provider)
    , store_(store)
    , tools_(make_tool_registry())
{
    current_model_ = provider_->route("code", "general");
}

auto AgentSession::send(const std::string& message) -> ProviderResult {
    // Record user message
    messages_.push_back({
        .role = Role::User,
        .content = message,
        .agent_id = agent_id_,
        .model = {},
        .timestamp = {},
    });
    emit(SessionEvent::MessageSent, message);

    // Build context and execute
    auto prompt = build_context();
    auto result = provider_->execute(current_model_, prompt);

    // Record assistant response
    messages_.push_back({
        .role = Role::Assistant,
        .content = result.output,
        .agent_id = agent_id_,
        .model = current_model_.model,
        .timestamp = {},
        .duration_ms = result.duration_ms,
    });
    emit(SessionEvent::ResponseReceived, result.output);

    return result;
}

void AgentSession::register_tool(ToolDeclaration decl, ToolHandler handler) {
    tools_->register_tool(std::move(decl), std::move(handler));
}

void AgentSession::remove_tool(const std::string& name) {
    tools_->remove_tool(name);
}

void AgentSession::set_system_prompt(const std::string& prompt) {
    system_prompt_ = prompt;
}

void AgentSession::append_system_context(const std::string& context) {
    if (!system_context_.empty()) system_context_ += "\n";
    system_context_ += context;
}

auto AgentSession::switch_model(const ModelSpec& spec) -> bool {
    bool ok = provider_->switch_model(spec);
    if (ok) {
        current_model_ = spec;
        emit(SessionEvent::ModelSwitched, spec.model);
    }
    return ok;
}

void AgentSession::branch(const std::string& name) {
    branch_id_ = name;
    emit(SessionEvent::BranchCreated, name);
}

auto AgentSession::compact(size_t keep_n) -> std::expected<void, std::string> {
    if (messages_.size() > keep_n) {
        messages_.erase(
            messages_.begin(),
            messages_.begin() +
                static_cast<ptrdiff_t>(messages_.size() - keep_n));
    }
    return {};
}

auto AgentSession::save() -> std::expected<void, std::string> {
    if (!store_) return std::unexpected("No session store configured");
    SessionSnapshot snap{
        .session_id = session_id_,
        .branch_id = branch_id_,
        .agent_id = agent_id_,
        .messages = messages_,
    };
    auto result = store_->save(snap);
    if (result.has_value())
        emit(SessionEvent::SessionSaved);
    return result;
}

auto AgentSession::load(const std::string& branch) -> std::expected<void, std::string> {
    if (!store_) return std::unexpected("No session store configured");
    auto result = store_->load(session_id_, branch);
    if (!result.has_value()) return std::unexpected(result.error());

    branch_id_ = result->branch_id;
    messages_ = std::move(result->messages);
    emit(SessionEvent::SessionLoaded);
    return {};
}

void AgentSession::on_event(SessionEventHandler handler) {
    event_handlers_.push_back(std::move(handler));
}

auto AgentSession::session_id() const -> const std::string& { return session_id_; }
auto AgentSession::agent_id() const -> const std::string& { return agent_id_; }
auto AgentSession::messages() const -> const std::vector<ConversationMessage>& { return messages_; }
auto AgentSession::current_branch() const -> const std::string& { return branch_id_; }

void AgentSession::emit(SessionEvent event, const std::string& detail) {
    for (const auto& handler : event_handlers_)
        handler(event, detail);
}

auto AgentSession::build_context() const -> std::string {
    std::ostringstream ctx;

    if (!system_prompt_.empty())
        ctx << system_prompt_ << "\n\n";
    if (!system_context_.empty())
        ctx << system_context_ << "\n\n";

    // Include tool declarations
    auto tools = tools_->list_tools();
    if (!tools.empty()) {
        ctx << "Available tools:\n";
        for (const auto& tool : tools)
            ctx << "- " << tool.name << ": " << tool.description << "\n";
        ctx << "\n";
    }

    // Include conversation history
    for (const auto& msg : messages_) {
        switch (msg.role) {
            case Role::User:      ctx << "User: "; break;
            case Role::Assistant: ctx << "Assistant: "; break;
            case Role::System:    ctx << "System: "; break;
        }
        ctx << msg.content << "\n\n";
    }

    return ctx.str();
}

} // namespace euxis::runtime
