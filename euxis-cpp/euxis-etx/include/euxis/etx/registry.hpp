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
    QString status;      // "idle", "running", "error"
    QString type;        // "agent", "squad", "combo"
    QString tier;        // "core", "fleet"
    QString version;
    QString activation;  // "core", "default", "on-demand", "specialist"
    QString prompt_path;
    QStringList tags;
    QStringList capability_tags;
};

struct SquadInfo {
    QString id;
    QString name;
    QString purpose;
    QString lead;
    QStringList members;
};

struct ComboInfo {
    QString id;
    QString name;
    QString description;
    QStringList chain;
};

struct PlaybookSummary {
    QString id;
    QString name;
    QString description;
    QString file_path;
    int phase_count = 0;
};

class FleetRegistry : public QObject {
    Q_OBJECT

public:
    explicit FleetRegistry(const QString& data_dir = {},
                           QObject* parent = nullptr);

    void refresh();
    [[nodiscard]] auto agents() const -> const std::vector<AgentInfo>&;
    [[nodiscard]] auto find(const QString& id) const -> const AgentInfo*;
    [[nodiscard]] auto filter(const QString& query) const -> std::vector<AgentInfo>;

    [[nodiscard]] auto squads() const -> const std::vector<SquadInfo>&;
    [[nodiscard]] auto combos() const -> const std::vector<ComboInfo>&;
    [[nodiscard]] auto playbooks() const -> const std::vector<PlaybookSummary>&;

    [[nodiscard]] auto find_squad(const QString& id) const -> const SquadInfo*;
    [[nodiscard]] auto filter_by_tier(const QString& tier) const -> std::vector<AgentInfo>;
    [[nodiscard]] auto filter_by_tag(const QString& tag) const -> std::vector<AgentInfo>;

    [[nodiscard]] auto agent_count() const -> int;
    [[nodiscard]] auto squad_count() const -> int;
    [[nodiscard]] auto combo_count() const -> int;

signals:
    void refreshed();

private:
    QString data_dir_;
    std::vector<AgentInfo> agents_;
    std::vector<SquadInfo> squads_;
    std::vector<ComboInfo> combos_;
    std::vector<PlaybookSummary> playbooks_;

    void load_agents();
    void load_squads();
    void load_playbooks();
    void load_defaults();
};

} // namespace euxis::etx
