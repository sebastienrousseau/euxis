#pragma once
#include <QObject>
#include <QString>
#include <QList>
#include <QDateTime>
#include <QStringList>

#include <euxis/cli/auth_profile_store.hpp>
#include <euxis/cli/provider_router.hpp>
#include <euxis/cli/provider_executor.hpp>
#include <euxis/cli/session.hpp>
#include <euxis/cli/pii_filter.hpp>

#include <QAtomicInt>

namespace euxis::etx {

class FleetRegistry;
class OAuthFlow;

/// A single message in the conversation history.
struct ChatMessage {
    /// The role of the message sender.
    enum Role { User, Assistant, System };
    Role role{User};
    QString agent_id;
    QString text;
    QString model;
    double duration_ms{0.0};
    QDateTime timestamp;
};

/// Core LLM interaction engine for the ETX GUI.
///
/// Manages conversation history, provider routing with auth profile rotation,
/// and asynchronous request execution. Emits signals for streaming responses,
/// provider fallback events, and cooldown notifications.
class ChatEngine : public QObject {
    Q_OBJECT

public:
    /// Construct a ChatEngine rooted at @p data_dir for config/state persistence.
    explicit ChatEngine(const QString& data_dir, FleetRegistry* registry,
                           QObject* parent = nullptr);
    ~ChatEngine() override;

    /// Send a user message to the active (or specified) agent.
    void send_message(const QString& text, const QString& agent_id = {});
    /// Switch the active agent used for subsequent messages.
    void set_active_agent(const QString& agent_id);
    /// Return the currently active agent identifier.
    [[nodiscard]] auto active_agent() const -> QString;
    /// Return the full conversation history.
    [[nodiscard]] auto conversation_history() const -> const QList<ChatMessage>&;
    /// Clear the conversation history.
    void clear_history();

    /// @name Provider management
    /// @{
    /// Override the auto-detected provider.
    void set_provider(const QString& provider);
    /// Return the current (possibly overridden) provider.
    [[nodiscard]] auto current_provider() const -> QString;
    /// Return the auto-detected provider from environment/config.
    [[nodiscard]] auto detected_provider() const -> QString;
    /// Return all providers that have at least one usable auth profile.
    [[nodiscard]] auto available_providers() const -> QStringList;
    /// Return a human-readable status summary of the active provider.
    [[nodiscard]] auto provider_status_text() const -> QString;
    /// @}

    /// @name Auth profile management
    /// @{
    /// Access the underlying auth profile store.
    [[nodiscard]] auto auth_store() -> cli::AuthProfileStore&;
    /// Return all auth profiles for a given provider.
    [[nodiscard]] auto auth_profiles_for(const QString& provider) -> QList<cli::AuthProfile>;
    /// Add a new API key profile.
    void add_api_key(const QString& provider, const QString& key, const QString& label = {});
    /// Remove an auth profile by id.
    void remove_auth_profile(const QString& id);
    /// @}

    /// @name OAuth login
    /// @{
    /// Initiate browser-based OAuth login for a provider.
    void start_login(const QString& provider);
    /// Access the OAuth flow object (creates on first call).
    [[nodiscard]] auto oauth_flow() -> OAuthFlow*;
    /// @}

    /// @name Lifecycle
    /// @{
    /// Gracefully shut down the engine, waiting for pending async tasks.
    void shutdown();
    /// @}

signals:
    /// Emitted when a provider begins generating a response.
    void response_started(const QString& agent_id, const QString& model);
    /// Emitted for each streaming text chunk.
    void response_chunk(const QString& text);
    /// Emitted when the full response has been received.
    void response_finished(const QString& full_text, const QString& model,
                           double duration_ms);
    /// Emitted when the request fails after all retries/fallbacks.
    void response_error(const QString& error);
    /// Emitted when the user explicitly changes the provider.
    void provider_changed(const QString& provider);
    /// Emitted when a provider fails and the engine falls back to another.
    void fallback_activated(const QString& from_provider, const QString& to_provider);
    /// Emitted when an auth profile enters cooldown after a failure.
    void profile_cooldown_started(const QString& profile_id, const QString& reason);

private:
    void execute_async(const QString& prompt, const QString& agent_id,
                       const cli::ModelSelection& selection);

    cli::ProviderRouter router_;
    cli::ProviderExecutor executor_;
    cli::AuthProfileStore auth_store_;
    cli::Session session_;
    FleetRegistry* registry_{nullptr};
    OAuthFlow* oauth_flow_{nullptr};
    QString active_agent_id_{"code-agent"};
    QList<ChatMessage> history_;
    QString data_dir_;
    QString override_provider_;  // Empty = auto-detect
    QAtomicInt active_tasks_{0};
};

} // namespace euxis::etx
