#include "euxis/scan/rule_engine.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <sstream>
#include <string>

namespace euxis::scan {

namespace {

/// Match context threaded through recursive evaluation. Carries
/// just enough graph/ast state for the new Inside pattern kind
/// to walk the AstChild parent chain. Literal / Composite /
/// Alternation / Negation only consume `text`; only Inside reaches
/// for the rest.
struct MatchContext {
    const euxis::cpg::Graph& graph;
    const euxis::parse::Ast& ast;
    euxis::cpg::NodeId current_node;
};

bool pattern_matches(const Pattern& p,
                     std::string_view text,
                     const MatchContext& ctx);

/// Inside semantics: the pattern matches when ANY AstChild
/// ancestor of `current_node` (excluding `current_node` itself)
/// has source text that satisfies the nested child pattern.
/// Bottom-up walk; bails the first time a child pattern matches.
bool inside_matches(const Pattern& inside,
                    const MatchContext& ctx) {
    if (inside.children.empty()) return false;
    const auto& child = inside.children.front();
    auto parent = ctx.graph.parent(ctx.current_node);
    while (parent.is_valid()) {
        const auto* parent_node = ctx.graph.get(parent);
        if (parent_node != nullptr) {
            auto parent_text = ctx.ast.node_text(parent_node->range);
            if (!parent_text.empty()) {
                MatchContext climbed{ctx.graph, ctx.ast, parent};
                if (pattern_matches(child, parent_text, climbed)) return true;
            }
        }
        parent = ctx.graph.parent(parent);
    }
    return false;
}

/// Recursive pattern evaluator.
///  - Literal     : straight substring search on `text`.
///  - Composite   : AND of children.
///  - Alternation : OR of children.
///  - Negation    : NOT of the (single) child.
///  - Inside      : child must match the text of an AstChild
///                  ancestor of `current_node`.
bool pattern_matches(const Pattern& p,
                     std::string_view text,
                     const MatchContext& ctx) {
    switch (p.kind) {
        case PatternKind::Literal:
            if (p.text.empty()) return false;
            return text.find(p.text) != std::string_view::npos;
        case PatternKind::Composite:
            if (p.children.empty()) return false;
            return std::all_of(p.children.begin(), p.children.end(),
                [&](const Pattern& c) { return pattern_matches(c, text, ctx); });
        case PatternKind::Alternation:
            if (p.children.empty()) return false;
            return std::any_of(p.children.begin(), p.children.end(),
                [&](const Pattern& c) { return pattern_matches(c, text, ctx); });
        case PatternKind::Negation:
            if (p.children.empty()) return true;  // vacuous-not = true
            return !pattern_matches(p.children.front(), text, ctx);
        case PatternKind::Inside:
            return inside_matches(p, ctx);
    }
    return false;
}

bool rule_matches_language(const Rule& r, euxis::parse::Language lang) {
    if (r.languages.empty()) return true;  // unconstrained
    return std::find(r.languages.begin(), r.languages.end(), lang) !=
           r.languages.end();
}

auto fnv1a(std::string_view a, std::string_view b,
           std::string_view c, std::string_view d) -> std::uint64_t {
    std::uint64_t h = 0xcbf29ce484222325ULL;
    auto absorb = [&](std::string_view s) {
        for (unsigned char ch : s) {
            h ^= ch;
            h *= 0x100000001b3ULL;
        }
    };
    absorb(a); absorb(b); absorb(c); absorb(d);
    return h;
}

auto stable_fingerprint(std::string_view rule_id,
                        std::string_view file,
                        std::string_view raw_kind,
                        std::string_view name) -> std::string {
    std::uint64_t h = fnv1a(rule_id, file, raw_kind, name);
    static constexpr std::array<char, 16> hex{
        '0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f',
    };
    auto append = [&](std::string& s, std::uint64_t v) {
        for (int i = 15; i >= 0; --i) {
            s.push_back(hex[(v >> (i * 4)) & 0xF]);
        }
    };
    std::string out;
    out.reserve(32);
    append(out, h);
    append(out, h ^ 0x5a5a5a5a5a5a5a5aULL);
    return out;
}

auto make_finding(const Rule& r,
                  const RulePack& pack,
                  const euxis::cpg::Node& node,
                  const std::string& file) -> euxis::security::Finding {
    using namespace euxis::security;
    Finding f;
    std::ostringstream rule_id;
    rule_id << "opengrep/" << pack.name << "/" << r.id;
    f.rule_id    = rule_id.str();
    f.message    = r.message;
    f.severity   = to_security_severity(r.severity);
    f.confidence = Confidence::Probable;
    f.source     = RuleSource::OpenGrep;

    SourceLocation loc;
    loc.path         = file;
    loc.start_line   = static_cast<int>(node.range.start_row + 1);
    loc.start_column = static_cast<int>(node.range.start_column + 1);
    loc.end_line     = static_cast<int>(node.range.end_row + 1);
    loc.end_column   = static_cast<int>(node.range.end_column + 1);
    f.primary_location = loc;

    if (r.metadata.primary_cwe) {
        f.cwe = CweRef{
            .id         = *r.metadata.primary_cwe,
            .short_name = "",
            .in_top_25_2025 = false,
            .top_25_rank    = 0,
        };
    }
    if (r.metadata.primary_owasp) {
        f.owasp = *r.metadata.primary_owasp;
    }
    f.compliance_taxa = r.metadata.references;
    f.stable_fingerprint =
        stable_fingerprint(f.rule_id, file, node.raw_kind, node.name);
    return f;
}

} // namespace

auto apply_rules(const RulePack& pack,
                 const euxis::parse::Ast& ast,
                 const euxis::cpg::Graph& graph,
                 const std::string& file) -> EngineResult {
    EngineResult result;
    auto& stats = result.stats;

    const auto lang = ast.language();
    stats.rules_evaluated = pack.rules.size();

    for (const auto& rule : pack.rules) {
        if (!rule_matches_language(rule, lang)) {
            ++stats.skipped_language_mismatch;
            continue;
        }
        for (const auto& node : graph.nodes()) {
            ++stats.nodes_visited;
            auto text = ast.node_text(node.range);
            if (text.empty()) continue;
            MatchContext ctx{graph, ast, node.id};
            if (!pattern_matches(rule.pattern, text, ctx)) continue;
            result.findings.push_back(make_finding(rule, pack, node, file));
            ++stats.findings_emitted;
        }
    }

    return result;
}

} // namespace euxis::scan
