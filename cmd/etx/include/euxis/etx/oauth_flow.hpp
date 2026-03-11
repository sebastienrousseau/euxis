#pragma once

#include <QObject>
#include <QString>
#include <QTcpServer>
#include <QNetworkAccessManager>

namespace euxis::etx {

/// Browser-based OAuth login flow with localhost callback server.
class OAuthFlow : public QObject {
    Q_OBJECT

public:
    explicit OAuthFlow(QObject* parent = nullptr);

    /// Start OAuth login for a provider. Opens the system browser.
    void start_login(const QString& provider);

    /// Cancel any in-progress login.
    void cancel();

signals:
    void login_succeeded(const QString& provider, const QString& access_token,
                         const QString& refresh_token, int64_t expires_at,
                         const QString& email);
    void login_failed(const QString& provider, const QString& error);

private:
    struct ProviderConfig {
        QString auth_url;
        QString token_url;
        QString client_id;
        QString scopes;
        QString callback_path;
    };

    static auto provider_config(const QString& provider) -> ProviderConfig;

    void handle_callback(const QByteArray& request_data);
    void exchange_code(const QString& code);
    static auto generate_code_verifier() -> QString;
    static auto compute_code_challenge(const QString& verifier) -> QString;
    static auto base64url_encode(const QByteArray& data) -> QString;

    QTcpServer* callback_server_{nullptr};
    QNetworkAccessManager* network_{nullptr};

    // PKCE state
    QString code_verifier_;
    QString code_challenge_;
    QString current_provider_;
    QString redirect_uri_;
    QString state_;
    ProviderConfig current_config_;
};

} // namespace euxis::etx
