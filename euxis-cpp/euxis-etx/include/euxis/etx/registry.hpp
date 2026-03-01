#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include <vector>

namespace euxis::etx {

struct AgentInfo {
    QString id;
    QString name;
    QString description;
    QString status;  // "idle", "running", "error"
    QString type;    // "agent", "squad", "combo"
};

class FleetRegistry : public QObject {
    Q_OBJECT

public:
    explicit FleetRegistry(QObject* parent = nullptr);

    void refresh();
    [[nodiscard]] auto agents() const -> const std::vector<AgentInfo>&;
    [[nodiscard]] auto find(const QString& id) const -> const AgentInfo*;
    [[nodiscard]] auto filter(const QString& query) const -> std::vector<AgentInfo>;

signals:
    void refreshed();

private:
    std::vector<AgentInfo> agents_;
    void load_defaults();
};

} // namespace euxis::etx
