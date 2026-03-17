#include <euxis/etx/registry.hpp>

#include <nlohmann/json.hpp>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

namespace euxis::etx {

using json = nlohmann::json;

// Helper: read role from prompt YAML frontmatter
static QString read_agent_role(const QString& data_dir, const QString& rel_path) {
    QString full_path = data_dir + "/" + rel_path;
    QFile file(full_path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};

    QTextStream in(&file);
    bool in_frontmatter = false;
    while (!in.atEnd()) {
        QString line = in.readLine();
        if (line.trimmed() == "---") {
            if (!in_frontmatter) {
                in_frontmatter = true;
                continue;
            }
            break; // end of frontmatter
        }
        if (in_frontmatter && line.startsWith("role:")) {
            return line.mid(5).trimmed();
        }
    }
    return {};
}

// Helper: title-case an id (e.g. "deep-researcher" -> "Deep Researcher")
static QString title_case(const QString& id) {
    QStringList parts = id.split('-');
    for (auto& p : parts) {
        if (!p.isEmpty()) {
            p[0] = p[0].toUpper();
        }
    }
    return parts.join(' ');
}

FleetRegistry::FleetRegistry(const QString& data_dir, QObject* parent)
    : QObject(parent)
    , data_dir_(data_dir)
{
    if (!data_dir_.isEmpty()) {
        load_agents();
        load_squads();
        load_playbooks();
    }
    if (agents_.empty()) {
        load_defaults();
    }
}

void FleetRegistry::refresh() {
    agents_.clear();
    squads_.clear();
    combos_.clear();
    playbooks_.clear();

    if (!data_dir_.isEmpty()) {
        load_agents();
        load_squads();
        load_playbooks();
    }
    if (agents_.empty()) {
        load_defaults();
    }
    emit refreshed();
}

auto FleetRegistry::agents() const -> const std::vector<AgentInfo>& {
    return agents_;
}

auto FleetRegistry::find(const QString& id) const -> const AgentInfo* {
    for (const auto& a : agents_) {
        if (a.id == id) return &a;
    }
    return nullptr;
}

auto FleetRegistry::filter(const QString& query) const -> std::vector<AgentInfo> {
    if (query.isEmpty()) return agents_;

    std::vector<AgentInfo> results;
    for (const auto& a : agents_) {
        if (a.name.contains(query, Qt::CaseInsensitive) ||
            a.description.contains(query, Qt::CaseInsensitive) ||
            a.id.contains(query, Qt::CaseInsensitive) ||
            a.tier.contains(query, Qt::CaseInsensitive) ||
            a.activation.contains(query, Qt::CaseInsensitive)) {
            results.push_back(a);
            continue;
        }
        // Search tags
        for (const auto& tag : a.tags) {
            if (tag.contains(query, Qt::CaseInsensitive)) {
                results.push_back(a);
                break;
            }
        }
    }
    return results;
}

auto FleetRegistry::squads() const -> const std::vector<SquadInfo>& {
    return squads_;
}

auto FleetRegistry::combos() const -> const std::vector<ComboInfo>& {
    return combos_;
}

auto FleetRegistry::playbooks() const -> const std::vector<PlaybookSummary>& {
    return playbooks_;
}

auto FleetRegistry::find_squad(const QString& id) const -> const SquadInfo* {
    for (const auto& s : squads_) {
        if (s.id == id) return &s;
    }
    return nullptr;
}

auto FleetRegistry::filter_by_tier(const QString& tier) const -> std::vector<AgentInfo> {
    std::vector<AgentInfo> results;
    for (const auto& a : agents_) {
        if (a.tier == tier) results.push_back(a);
    }
    return results;
}

auto FleetRegistry::filter_by_tag(const QString& tag) const -> std::vector<AgentInfo> {
    std::vector<AgentInfo> results;
    for (const auto& a : agents_) {
        if (a.tags.contains(tag)) results.push_back(a);
    }
    return results;
}

auto FleetRegistry::agent_count() const -> int {
    return static_cast<int>(agents_.size());
}

auto FleetRegistry::squad_count() const -> int {
    return static_cast<int>(squads_.size());
}

auto FleetRegistry::combo_count() const -> int {
    return static_cast<int>(combos_.size());
}

void FleetRegistry::load_agents() {
    QString path = data_dir_ + "/agents/registry.json";
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return;

    try {
        auto doc = json::parse(file.readAll().toStdString());
        if (!doc.contains("agents") || !doc["agents"].is_array()) return;

        for (const auto& entry : doc["agents"]) {
            AgentInfo info;
            info.id = QString::fromStdString(entry.value("id", ""));
            info.tier = QString::fromStdString(entry.value("tier", "fleet"));
            info.version = QString::fromStdString(entry.value("version", ""));
            info.activation = QString::fromStdString(entry.value("activation", "default"));
            info.prompt_path = QString::fromStdString(entry.value("path", ""));

            // Title-case the id for display name
            info.name = title_case(info.id);

            // Read role from prompt file as description
            QString role = read_agent_role(data_dir_, info.prompt_path);
            info.description = role.isEmpty() ? info.name : role;

            // Default status: idle for all real agents
            info.status = "idle";
            info.type = "agent";

            // Tags
            if (entry.contains("tags") && entry["tags"].is_array()) {
                for (const auto& t : entry["tags"]) {
                    info.tags.append(QString::fromStdString(t.get<std::string>()));
                }
            }

            // Capability tags
            if (entry.contains("capability_tags") && entry["capability_tags"].is_array()) {
                for (const auto& ct : entry["capability_tags"]) {
                    info.capability_tags.append(QString::fromStdString(ct.get<std::string>()));
                }
            }

            agents_.push_back(std::move(info));
        }
    } catch (const json::exception&) {
        // Fall through — load_defaults() will be called if agents_ is empty
    }
}

void FleetRegistry::load_squads() {
    QString path = data_dir_ + "/agents/squads.json";
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return;

    try {
        auto doc = json::parse(file.readAll().toStdString());

        // Squads
        if (doc.contains("squads") && doc["squads"].is_array()) {
            for (const auto& entry : doc["squads"]) {
                SquadInfo squad;
                squad.id = QString::fromStdString(entry.value("id", ""));
                squad.name = QString::fromStdString(entry.value("name", ""));
                squad.purpose = QString::fromStdString(entry.value("purpose", ""));
                squad.lead = QString::fromStdString(entry.value("lead", ""));

                if (entry.contains("members") && entry["members"].is_array()) {
                    for (const auto& m : entry["members"]) {
                        squad.members.append(QString::fromStdString(m.get<std::string>()));
                    }
                }
                squads_.push_back(std::move(squad));
            }
        }

        // Combos
        if (doc.contains("combos") && doc["combos"].is_array()) {
            for (const auto& entry : doc["combos"]) {
                ComboInfo combo;
                combo.id = QString::fromStdString(entry.value("id", ""));
                combo.name = QString::fromStdString(entry.value("name", ""));
                combo.description = QString::fromStdString(entry.value("description", ""));

                if (entry.contains("chain") && entry["chain"].is_array()) {
                    for (const auto& c : entry["chain"]) {
                        combo.chain.append(QString::fromStdString(c.get<std::string>()));
                    }
                }
                combos_.push_back(std::move(combo));
            }
        }
    } catch (const json::exception&) {
        // Ignore parse errors
    }
}

void FleetRegistry::load_playbooks() {
    QString pb_dir = data_dir_ + "/config/playbooks";
    QDir dir(pb_dir);
    if (!dir.exists()) return;

    QStringList filters = {"*.json"};
    auto entries = dir.entryInfoList(filters, QDir::Files, QDir::Name);

    for (const auto& fi : entries) {
        QFile file(fi.absoluteFilePath());
        if (!file.open(QIODevice::ReadOnly)) continue;

        try {
            auto doc = json::parse(file.readAll().toStdString());
            PlaybookSummary pb;
            pb.id = QString::fromStdString(doc.value("id", fi.baseName().toStdString()));
            pb.name = QString::fromStdString(doc.value("name", pb.id.toStdString()));
            pb.description = QString::fromStdString(doc.value("description", ""));
            pb.file_path = fi.absoluteFilePath();
            if (doc.contains("phases") && doc["phases"].is_array()) {
                pb.phase_count = static_cast<int>(doc["phases"].size());
            }
            playbooks_.push_back(std::move(pb));
        } catch (const json::exception&) {
            continue;
        }
    }
}

void FleetRegistry::load_defaults() {
    agents_.push_back({
        "code-agent", "Code Agent",
        "Autonomous code generation, review, and refactoring with WASM sandboxing",
        "idle", "agent", "core", "v0.0.3", "core", {}, {}, {}
    });
    agents_.push_back({
        "research-agent", "Research Agent",
        "Web research, summarization, and knowledge synthesis with citation tracking",
        "running", "agent", "fleet", "v0.0.3", "default", {}, {}, {}
    });
    agents_.push_back({
        "security-agent", "Security Agent",
        "Supply chain verification, vulnerability scanning, and threat assessment",
        "idle", "agent", "fleet", "v0.0.3", "default", {}, {}, {}
    });
    agents_.push_back({
        "bridge-squad", "Bridge Squad",
        "OpenClaw/ClawHub interop squad for skill import, verification, and deployment",
        "idle", "squad", "fleet", "v0.0.3", "default", {}, {}, {}
    });
    agents_.push_back({
        "ops-combo", "Ops Combo",
        "Combined operations: deployment, monitoring, incident response, and rollback",
        "running", "combo", "fleet", "v0.0.3", "default", {}, {}, {}
    });
    agents_.push_back({
        "inference-agent", "Inference Agent",
        "Local and remote LLM inference routing with FinOps cost optimization",
        "idle", "agent", "fleet", "v0.0.3", "default", {}, {}, {}
    });
    agents_.push_back({
        "identity-agent", "Identity Agent",
        "KYA/DID identity management, ERC-8004 registration, and credential issuance",
        "error", "agent", "fleet", "v0.0.3", "default", {}, {}, {}
    });
    agents_.push_back({
        "memory-agent", "Memory Agent",
        "AES-256-GCM encrypted memory with checkpoint/resume and context windowing",
        "idle", "agent", "fleet", "v0.0.3", "default", {}, {}, {}
    });
}

} // namespace euxis::etx
