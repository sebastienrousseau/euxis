#include <euxis/etx/chat_engine.hpp>
#include <euxis/etx/oauth_flow.hpp>
#include <euxis/etx/registry.hpp>

#include <QtConcurrent/QtConcurrent>
#include <QRegularExpression>

namespace euxis::etx {

ChatEngine::ChatEngine(const QString& data_dir, FleetRegistry* registry, QObject* parent)
    : QObject(parent)
    , router_(data_dir.toStdString())
    , executor_(data_dir.toStdString())
    , auth_store_(data_dir.toStdString())
    , session_(data_dir.toStdString())
    , registry_(registry)
    , data_dir_(data_dir)
{
    // Auto-import auth from Claude Code and Gemini CLI
    auth_store_.auto_import_claude_oauth();
    auth_store_.auto_import_gemini_oauth();
    auth_store_.import_env_vars();

    // Create OAuth flow
    oauth_flow_ = new OAuthFlow(this);
    connect(oauth_flow_, &OAuthFlow::login_succeeded, this,
        [this](const QString& provider, const QString& access_token,
               const QString& refresh_token, int64_t expires_at,
               const QString& email) {
            auth_store_.add_oauth(provider.toStdString(),
                                  access_token.toStdString(),
                                  refresh_token.toStdString(),
                                  expires_at,
                                  email.toStdString());
            auth_store_.save();
        });
}

ChatEngine::~ChatEngine() {
    shutdown();
}

void ChatEngine::send_message(const QString& text, const QString& agent_id) {
    QString target_agent = agent_id;

    // Check for @agent-name prefix routing
    static const QRegularExpression at_prefix(R"(^@([\w-]+)\s+)");
    auto match = at_prefix.match(text);
    QString prompt_text = text;
    if (match.hasMatch()) {
        target_agent = match.captured(1);
        prompt_text = text.mid(match.capturedLength());
    }

    if (target_agent.isEmpty()) {
        target_agent = active_agent_id_;
    }

    // Apply PII filtering
    std::string filtered = cli::PiiFilter::redact(prompt_text.toStdString());
    prompt_text = QString::fromStdString(filtered);

    // Record user message
    ChatMessage user_msg;
    user_msg.role = ChatMessage::User;
    user_msg.agent_id = target_agent;
    user_msg.text = text;
    user_msg.timestamp = QDateTime::currentDateTime();
    history_.append(user_msg);

    // Route to model — use task content for tier analysis, not agent_id
    auto selection = router_.route("code", prompt_text.toStdString());

    // Override provider if user has selected one
    if (!override_provider_.isEmpty()) {
        selection.provider = override_provider_.toStdString();
    }

    emit response_started(target_agent,
                          QString::fromStdString(selection.model));

    execute_async(prompt_text, target_agent, selection);
}

void ChatEngine::execute_async(const QString& prompt, const QString& agent_id,
                                const cli::ModelSelection& selection) {
    active_tasks_.fetchAndAddRelaxed(1);

    // Resolve agent prompt path from registry
    std::string system_prompt;
    if (registry_) {
        const AgentInfo* agent = registry_->find(agent_id);
        if (agent && !agent->prompt_path.isEmpty()) {
            system_prompt = cli::ProviderExecutor::load_agent_prompt(
                session_.euxis_home(),
                agent->prompt_path.toStdString());
        }
    }

    // Fallback if registry resolution fails
    if (system_prompt.empty()) {
        system_prompt = cli::ProviderExecutor::load_agent_prompt(
            data_dir_.toStdString(),
            "agents/" + agent_id.toStdString() + "/prompt.md");
    }

    std::string memory_ctx = session_.get_memory_context(agent_id.toStdString());

    std::string full_prompt = cli::ProviderExecutor::build_prompt(
        system_prompt, prompt.toStdString(), memory_ctx);

    const auto& sel = selection;
    auto* exec = &executor_;
    auto* store = &auth_store_;
    auto* router = &router_;
    auto* engine = this;
    const auto& aid = agent_id;

    (void)QtConcurrent::run([exec, store, router, sel, full_prompt, engine, aid]() {
        auto original_provider = sel.provider;

        // Try resolving auth from the profile store
        auto auth = store->resolve_with_fallback(sel.provider);

        // Track if we switched provider
        std::string actual_provider = sel.provider;
        if (auth.has_value() && auth->provider != sel.provider) {
            actual_provider = auth->provider;
        }

        // Build effective selection
        auto effective_sel = sel;
        if (auth.has_value() && auth->provider != sel.provider) {
            effective_sel.provider = auth->provider;
        }

        auto response = exec->execute(effective_sel, full_prompt, 120, auth);

        if (response.success) {
            if (auth.has_value()) {
                store->report_success(auth->profile_id);
                store->pin_session(auth->profile_id);
            }

            // Notify about fallback if provider changed
            if (actual_provider != original_provider) {
                QMetaObject::invokeMethod(engine,
                    [engine, original_provider, actual_provider]() {
                        emit engine->fallback_activated(
                            QString::fromStdString(original_provider),
                            QString::fromStdString(actual_provider));
                    }, Qt::QueuedConnection);
            }

            QString output = QString::fromStdString(response.output).trimmed();
            QMetaObject::invokeMethod(engine, [engine, output, effective_sel, response, aid]() {
                ChatMessage msg;
                msg.role = ChatMessage::Assistant;
                msg.agent_id = aid;
                msg.text = output;
                msg.model = QString::fromStdString(effective_sel.model);
                msg.duration_ms = response.duration_ms;
                msg.timestamp = QDateTime::currentDateTime();
                engine->history_.append(msg);

                emit engine->response_finished(
                    output,
                    QString::fromStdString(effective_sel.model),
                    response.duration_ms);
            }, Qt::QueuedConnection);
        } else {
            // On failure with a classified reason, report and try rotation
            if (response.failure_reason.has_value() && auth.has_value()) {
                store->report_failure(auth->profile_id, *response.failure_reason);

                auto reason_str = *response.failure_reason == cli::CooldownReason::RateLimit
                    ? "rate_limit" : (*response.failure_reason == cli::CooldownReason::BillingError
                    ? "billing" : "auth_error");
                auto pid = auth->profile_id;

                QMetaObject::invokeMethod(engine, [engine, pid, reason_str]() {
                    emit engine->profile_cooldown_started(
                        QString::fromStdString(pid),
                        QString::fromStdString(reason_str));
                }, Qt::QueuedConnection);

                // Try next profile via rotation
                auto next_auth = store->resolve_with_fallback(sel.provider);
                if (next_auth.has_value() && next_auth->profile_id != auth->profile_id) {
                    auto retry_sel = sel;
                    if (next_auth->provider != sel.provider) {
                        retry_sel.provider = next_auth->provider;
                    }
                    auto retry_response = exec->execute(retry_sel, full_prompt, 120, next_auth);
                    if (retry_response.success) {
                        store->report_success(next_auth->profile_id);
                        store->pin_session(next_auth->profile_id);

                        QString output = QString::fromStdString(retry_response.output).trimmed();
                        QMetaObject::invokeMethod(engine,
                            [engine, output, retry_sel, retry_response, aid]() {
                                ChatMessage msg;
                                msg.role = ChatMessage::Assistant;
                                msg.agent_id = aid;
                                msg.text = output;
                                msg.model = QString::fromStdString(retry_sel.model);
                                msg.duration_ms = retry_response.duration_ms;
                                msg.timestamp = QDateTime::currentDateTime();
                                engine->history_.append(msg);

                                emit engine->response_finished(
                                    output,
                                    QString::fromStdString(retry_sel.model),
                                    retry_response.duration_ms);
                            }, Qt::QueuedConnection);
                        engine->active_tasks_.fetchAndAddRelaxed(-1);
                        return;
                    }
                }

                // Try model fallback chain
                auto fallback_models = router->model_fallback_chain(sel.model);
                for (const auto& fb_sel : fallback_models) {
                    auto fb_auth = store->resolve(fb_sel.provider);
                    if (!fb_auth.has_value()) continue;

                    auto fb_response = exec->execute(fb_sel, full_prompt, 120, fb_auth);
                    if (fb_response.success) {
                        store->report_success(fb_auth->profile_id);

                        QMetaObject::invokeMethod(engine,
                            [engine, original_provider, fb_sel]() {
                                emit engine->fallback_activated(
                                    QString::fromStdString(original_provider),
                                    QString::fromStdString(fb_sel.provider));
                            }, Qt::QueuedConnection);

                        QString output = QString::fromStdString(fb_response.output).trimmed();
                        QMetaObject::invokeMethod(engine,
                            [engine, output, fb_sel, fb_response, aid]() {
                                ChatMessage msg;
                                msg.role = ChatMessage::Assistant;
                                msg.agent_id = aid;
                                msg.text = output;
                                msg.model = QString::fromStdString(fb_sel.model);
                                msg.duration_ms = fb_response.duration_ms;
                                msg.timestamp = QDateTime::currentDateTime();
                                engine->history_.append(msg);

                                emit engine->response_finished(
                                    output,
                                    QString::fromStdString(fb_sel.model),
                                    fb_response.duration_ms);
                            }, Qt::QueuedConnection);
                        engine->active_tasks_.fetchAndAddRelaxed(-1);
                        return;
                    }
                }
            }

            // All attempts failed
            QString error = QString::fromStdString(response.error).trimmed();
            QString detail;
            if (response.exit_code == 127) {
                detail = "Provider CLI not found on PATH. "
                         "Install the provider or set an API key.";
            } else if (response.exit_code == -1) {
                detail = "Request timed out.";
            } else {
                detail = "Provider '" + QString::fromStdString(sel.provider)
                         + "' failed (exit " + QString::number(response.exit_code) + ")";
            }
            if (!error.isEmpty()) {
                detail += "\n" + error;
            }
            QMetaObject::invokeMethod(engine, [engine, detail]() {
                emit engine->response_error(detail);
            }, Qt::QueuedConnection);
        }
        engine->active_tasks_.fetchAndAddRelaxed(-1);
    });
}

void ChatEngine::shutdown() {
    // Wait for pending background tasks
    while (active_tasks_.loadRelaxed() > 0) {
        QThread::msleep(10);
        QCoreApplication::processEvents();
    }
}

void ChatEngine::set_active_agent(const QString& agent_id) {
    active_agent_id_ = agent_id;
}

auto ChatEngine::active_agent() const -> QString {
    return active_agent_id_;
}

auto ChatEngine::conversation_history() const -> const QList<ChatMessage>& {
    return history_;
}

void ChatEngine::clear_history() {
    history_.clear();
}

void ChatEngine::set_provider(const QString& provider) {
    override_provider_ = provider;
    if (provider.isEmpty()) {
        (void)router_.detect_provider();
    }
    emit provider_changed(current_provider());
}

auto ChatEngine::current_provider() const -> QString {
    if (!override_provider_.isEmpty()) return override_provider_;
    return QString::fromStdString(router_.detect_provider());
}

auto ChatEngine::detected_provider() const -> QString {
    return QString::fromStdString(router_.detect_provider());
}

auto ChatEngine::available_providers() const -> QStringList {
    QStringList result;
    auto providers = router_.available_providers();
    for (const auto& p : providers) {
        result.append(QString::fromStdString(p));
    }
    return result;
}

auto ChatEngine::provider_status_text() const -> QString {
    auto provider = current_provider();
    auto available = available_providers();
    return QString("Provider: %1 | Available: %2")
        .arg(provider, available.isEmpty() ? "none" : available.join(", "));
}

// --- Auth profile management ---

auto ChatEngine::auth_store() -> cli::AuthProfileStore& {
    return auth_store_;
}

auto ChatEngine::auth_profiles_for(const QString& provider) -> QList<cli::AuthProfile> {
    auto profiles = auth_store_.profiles_for(provider.toStdString());
    QList<cli::AuthProfile> result;
    for (auto& p : profiles) {
        result.append(std::move(p));
    }
    return result;
}

void ChatEngine::add_api_key(const QString& provider, const QString& key,
                              const QString& label) {
    auth_store_.add_api_key(provider.toStdString(), key.toStdString(),
                            label.toStdString());
    auth_store_.save();
}

void ChatEngine::remove_auth_profile(const QString& id) {
    auth_store_.remove_profile(id.toStdString());
    auth_store_.save();
}

void ChatEngine::start_login(const QString& provider) {
    // Instead of launching an OAuth flow with unregistered client IDs,
    // attempt to import credentials from locally-installed CLI tools.
    bool imported = false;

    if (provider == "anthropic") {
        auto before = auth_store_.profiles_for("anthropic").size();
        auth_store_.auto_import_claude_oauth();
        imported = auth_store_.profiles_for("anthropic").size() > before;

        if (!imported) {
            // Check if we already have anthropic profiles
            imported = !auth_store_.profiles_for("anthropic").empty();
        }

        if (imported) {
            auth_store_.save();
            if (oauth_flow_) {
                emit oauth_flow_->login_succeeded(provider, {}, {}, 0, {});
            }
            return;
        }
    } else if (provider == "gemini" || provider == "google") {
        auto before = auth_store_.profiles_for("gemini").size();
        auth_store_.auto_import_gemini_oauth();
        imported = auth_store_.profiles_for("gemini").size() > before;

        if (!imported) {
            imported = !auth_store_.profiles_for("gemini").empty();
        }

        if (imported) {
            auth_store_.save();
            if (oauth_flow_) {
                emit oauth_flow_->login_succeeded(provider, {}, {}, 0, {});
            }
            return;
        }
    }

    // No local credentials found — show helpful error instead of opening
    // a browser with invalid client_id (ETX does not have registered OAuth apps)
    if (oauth_flow_) {
        QString msg;
        if (provider == "anthropic") {
            msg = "No Anthropic credentials found. Install Claude Code "
                  "(npm install -g @anthropic-ai/claude-code) and authenticate, "
                  "or add an API key manually.";
        } else {
            msg = "No Google credentials found. Install Gemini CLI "
                  "(pip install google-genai) and authenticate, "
                  "or add an API key manually.";
        }
        emit oauth_flow_->login_failed(provider, msg);
    }
}

auto ChatEngine::oauth_flow() -> OAuthFlow* {
    return oauth_flow_;
}

} // namespace euxis::etx
