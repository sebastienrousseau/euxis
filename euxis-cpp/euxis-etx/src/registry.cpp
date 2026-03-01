#include <euxis/etx/registry.hpp>

namespace euxis::etx {

FleetRegistry::FleetRegistry(QObject* parent)
    : QObject(parent)
{
    load_defaults();
}

void FleetRegistry::refresh() {
    load_defaults();
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
            a.id.contains(query, Qt::CaseInsensitive)) {
            results.push_back(a);
        }
    }
    return results;
}

void FleetRegistry::load_defaults() {
    agents_.clear();

    agents_.push_back({
        "code-agent",
        "Code Agent",
        "Autonomous code generation, review, and refactoring with WASM sandboxing",
        "idle",
        "agent"
    });

    agents_.push_back({
        "research-agent",
        "Research Agent",
        "Web research, summarization, and knowledge synthesis with citation tracking",
        "running",
        "agent"
    });

    agents_.push_back({
        "security-agent",
        "Security Agent",
        "Supply chain verification, vulnerability scanning, and threat assessment",
        "idle",
        "agent"
    });

    agents_.push_back({
        "bridge-squad",
        "Bridge Squad",
        "OpenClaw/ClawHub interop squad for skill import, verification, and deployment",
        "idle",
        "squad"
    });

    agents_.push_back({
        "ops-combo",
        "Ops Combo",
        "Combined operations: deployment, monitoring, incident response, and rollback",
        "running",
        "combo"
    });

    agents_.push_back({
        "inference-agent",
        "Inference Agent",
        "Local and remote LLM inference routing with FinOps cost optimization",
        "idle",
        "agent"
    });

    agents_.push_back({
        "identity-agent",
        "Identity Agent",
        "KYA/DID identity management, ERC-8004 registration, and credential issuance",
        "error",
        "agent"
    });

    agents_.push_back({
        "memory-agent",
        "Memory Agent",
        "AES-256-GCM encrypted memory with checkpoint/resume and context windowing",
        "idle",
        "agent"
    });
}

} // namespace euxis::etx
