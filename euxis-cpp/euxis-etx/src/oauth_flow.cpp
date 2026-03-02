#include <euxis/etx/oauth_flow.hpp>

#include <QCryptographicHash>
#include <QDesktopServices>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QRandomGenerator>
#include <QTcpSocket>
#include <QUrl>
#include <QUrlQuery>

namespace euxis::etx {

OAuthFlow::OAuthFlow(QObject* parent)
    : QObject(parent)
    , network_(new QNetworkAccessManager(this))
{
}

auto OAuthFlow::provider_config(const QString& provider) -> ProviderConfig {
    // NOTE: ETX does not have registered OAuth applications with Anthropic or
    // Google. Direct OAuth flow is not supported. Use ChatEngine::start_login()
    // which imports credentials from Claude Code / Gemini CLI instead.
    //
    // To register your own OAuth app:
    //   Anthropic: https://console.anthropic.com/settings/oauth
    //   Google: https://console.cloud.google.com/apis/credentials
    //
    // Then set EUXIS_ANTHROPIC_CLIENT_ID / EUXIS_GOOGLE_CLIENT_ID env vars.

    if (provider == "anthropic") {
        auto client_id = QString::fromLocal8Bit(
            qgetenv("EUXIS_ANTHROPIC_CLIENT_ID"));
        if (client_id.isEmpty()) return {};  // No registered client
        return {
            .auth_url = "https://claude.ai/oauth/authorize",
            .token_url = "https://console.anthropic.com/v1/oauth/token",
            .client_id = client_id,
            .scopes = "user:inference user:profile",
            .callback_path = "/callback",
        };
    }
    if (provider == "gemini" || provider == "google") {
        auto client_id = QString::fromLocal8Bit(
            qgetenv("EUXIS_GOOGLE_CLIENT_ID"));
        if (client_id.isEmpty()) return {};  // No registered client
        return {
            .auth_url = "https://accounts.google.com/o/oauth2/v2/auth",
            .token_url = "https://oauth2.googleapis.com/token",
            .client_id = client_id,
            .scopes = "https://www.googleapis.com/auth/cloud-platform "
                      "https://www.googleapis.com/auth/userinfo.email openid",
            .callback_path = "/oauth2callback",
        };
    }
    return {};
}

void OAuthFlow::start_login(const QString& provider) {
    cancel();

    current_provider_ = provider;
    current_config_ = provider_config(provider);
    if (current_config_.auth_url.isEmpty()) {
        emit login_failed(provider, "Unsupported OAuth provider: " + provider);
        return;
    }

    // Generate PKCE
    code_verifier_ = generate_code_verifier();
    code_challenge_ = compute_code_challenge(code_verifier_);

    // Generate state for CSRF protection
    QByteArray state_bytes(32, '\0');
    QRandomGenerator::global()->fillRange(
        reinterpret_cast<quint32*>(state_bytes.data()),
        state_bytes.size() / static_cast<int>(sizeof(quint32)));
    state_ = base64url_encode(state_bytes);

    // Start callback server
    callback_server_ = new QTcpServer(this);
    if (!callback_server_->listen(QHostAddress::LocalHost)) {
        emit login_failed(provider, "Failed to start callback server");
        return;
    }

    redirect_uri_ = QString("http://localhost:%1%2")
        .arg(callback_server_->serverPort())
        .arg(current_config_.callback_path);

    connect(callback_server_, &QTcpServer::newConnection, this, [this]() {
        auto* socket = callback_server_->nextPendingConnection();
        connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
            handle_callback(socket->readAll());

            // Send response to browser
            QByteArray response =
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/html\r\n"
                "Connection: close\r\n\r\n"
                "<html><body style='font-family:system-ui;text-align:center;"
                "padding:60px;background:#1a1a2e;color:#e0e0e0'>"
                "<h2>Authentication successful</h2>"
                "<p>You can close this tab and return to Euxis ETX.</p>"
                "</body></html>";
            socket->write(response);
            socket->flush();
            socket->disconnectFromHost();
        });
    });

    // Build auth URL
    QUrl auth_url(current_config_.auth_url);
    QUrlQuery query;
    query.addQueryItem("client_id", current_config_.client_id);
    query.addQueryItem("redirect_uri", redirect_uri_);
    query.addQueryItem("response_type", "code");
    query.addQueryItem("scope", current_config_.scopes);
    query.addQueryItem("state", state_);
    query.addQueryItem("code_challenge", code_challenge_);
    query.addQueryItem("code_challenge_method", "S256");
    auth_url.setQuery(query);

    if (qEnvironmentVariableIsEmpty("EUXIS_TEST_SKIP_BROWSER")) {
        QDesktopServices::openUrl(auth_url);
    }
}

void OAuthFlow::cancel() {
    if (callback_server_) {
        callback_server_->close();
        callback_server_->deleteLater();
        callback_server_ = nullptr;
    }
    code_verifier_.clear();
    code_challenge_.clear();
    state_.clear();
}

void OAuthFlow::handle_callback(const QByteArray& request_data) {
    // Parse GET request line
    QString request = QString::fromUtf8(request_data);
    auto first_line = request.section('\n', 0, 0);
    auto path = first_line.section(' ', 1, 1);

    QUrl url("http://localhost" + path);
    QUrlQuery query(url);

    // Verify state
    if (query.queryItemValue("state") != state_) {
        emit login_failed(current_provider_, "OAuth state mismatch (possible CSRF)");
        cancel();
        return;
    }

    // Check for error
    if (query.hasQueryItem("error")) {
        emit login_failed(current_provider_,
                          "OAuth error: " + query.queryItemValue("error_description"));
        cancel();
        return;
    }

    QString code = query.queryItemValue("code");
    if (code.isEmpty()) {
        emit login_failed(current_provider_, "No authorization code received");
        cancel();
        return;
    }

    exchange_code(code);
}

void OAuthFlow::exchange_code(const QString& code) {
    QUrl token_url(current_config_.token_url);

    QUrlQuery post_data;
    post_data.addQueryItem("grant_type", "authorization_code");
    post_data.addQueryItem("code", code);
    post_data.addQueryItem("redirect_uri", redirect_uri_);
    post_data.addQueryItem("client_id", current_config_.client_id);
    post_data.addQueryItem("code_verifier", code_verifier_);

    QNetworkRequest request(token_url);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      "application/x-www-form-urlencoded");

    auto* reply = network_->post(request, post_data.toString(QUrl::FullyEncoded).toUtf8());

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            emit login_failed(current_provider_,
                              "Token exchange failed: " + reply->errorString());
            cancel();
            return;
        }

        auto body = reply->readAll();
        auto doc = QJsonDocument::fromJson(body);
        auto obj = doc.object();

        QString access_token = obj["access_token"].toString();
        QString refresh_token = obj["refresh_token"].toString();
        int64_t expires_in = obj["expires_in"].toInteger(3600);
        int64_t expires_at = QDateTime::currentMSecsSinceEpoch() + expires_in * 1000;

        // Try to extract email from id_token (Google) or profile
        QString email;
        if (obj.contains("id_token")) {
            // Decode JWT payload (middle segment)
            auto id_token = obj["id_token"].toString();
            auto parts = id_token.split('.');
            if (parts.size() >= 2) {
                auto payload = QJsonDocument::fromJson(
                    QByteArray::fromBase64(parts[1].toUtf8(),
                                           QByteArray::Base64UrlEncoding));
                if (!payload.isNull()) {
                    email = payload.object()["email"].toString();
                }
            }
        }

        if (access_token.isEmpty()) {
            emit login_failed(current_provider_, "No access token in response");
        } else {
            emit login_succeeded(current_provider_, access_token,
                                 refresh_token, expires_at, email);
        }

        cancel();
    });
}

auto OAuthFlow::generate_code_verifier() -> QString {
    QByteArray bytes(64, '\0');
    QRandomGenerator::global()->fillRange(
        reinterpret_cast<quint32*>(bytes.data()),
        bytes.size() / static_cast<int>(sizeof(quint32)));
    return base64url_encode(bytes);
}

auto OAuthFlow::compute_code_challenge(const QString& verifier) -> QString {
    auto hash = QCryptographicHash::hash(verifier.toUtf8(),
                                          QCryptographicHash::Sha256);
    return base64url_encode(hash);
}

auto OAuthFlow::base64url_encode(const QByteArray& data) -> QString {
    return QString::fromLatin1(
        data.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}

} // namespace euxis::etx
