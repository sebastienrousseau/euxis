#include "euxis/parse/ast.hpp"

#include <tree_sitter/api.h>

#include <cstring>
#include <string>
#include <vector>

namespace euxis::parse {

struct Ast::Impl {
    Language lang{Language::C};
    TSTree* tree{nullptr};
    std::string source;  // owned bytes the tree references

    Impl() = default;
    ~Impl() {
        if (tree != nullptr) ts_tree_delete(tree);
    }
    Impl(const Impl&)            = delete;
    Impl& operator=(const Impl&) = delete;
    Impl(Impl&&) noexcept        = default;
    Impl& operator=(Impl&&) noexcept = default;
};

namespace {

SourceRange to_range(TSNode node) noexcept {
    auto start_point = ts_node_start_point(node);
    auto end_point   = ts_node_end_point(node);
    return SourceRange{
        .start_row    = start_point.row,
        .start_column = start_point.column,
        .end_row      = end_point.row,
        .end_column   = end_point.column,
        .start_byte   = ts_node_start_byte(node),
        .end_byte     = ts_node_end_byte(node),
    };
}

std::string_view node_kind(TSNode node) noexcept {
    const char* type = ts_node_type(node);
    return type != nullptr ? std::string_view{type} : std::string_view{};
}

} // namespace

Ast::Ast() noexcept  = default;
Ast::Ast(Ast&&) noexcept = default;
Ast& Ast::operator=(Ast&&) noexcept = default;
Ast::~Ast() = default;

auto Ast::language() const noexcept -> Language {
    return impl_ != nullptr ? impl_->lang : Language::C;
}

auto Ast::source() const noexcept -> std::string_view {
    return impl_ != nullptr
        ? std::string_view{impl_->source}
        : std::string_view{};
}

auto Ast::has_errors() const noexcept -> bool {
    if (impl_ == nullptr || impl_->tree == nullptr) return false;
    return ts_node_has_error(ts_tree_root_node(impl_->tree));
}

auto Ast::root_range() const noexcept -> SourceRange {
    if (impl_ == nullptr || impl_->tree == nullptr) return SourceRange{};
    return to_range(ts_tree_root_node(impl_->tree));
}

auto Ast::root_kind() const noexcept -> std::string_view {
    if (impl_ == nullptr || impl_->tree == nullptr) return {};
    return node_kind(ts_tree_root_node(impl_->tree));
}

auto Ast::root_named_child_count() const noexcept -> std::uint32_t {
    if (impl_ == nullptr || impl_->tree == nullptr) return 0;
    return ts_node_named_child_count(ts_tree_root_node(impl_->tree));
}

namespace {

bool visit_recursive(TSNode node, const Ast::Visitor& visit) {
    if (ts_node_is_named(node)) {
        bool descend = visit(node_kind(node), to_range(node));
        if (!descend) return true;
    }
    std::uint32_t child_count = ts_node_child_count(node);
    for (std::uint32_t i = 0; i < child_count; ++i) {
        if (!visit_recursive(ts_node_child(node, i), visit)) {
            return false;
        }
    }
    return true;
}

std::size_t count_named_recursive(TSNode node) {
    std::size_t total = ts_node_is_named(node) ? 1U : 0U;
    std::uint32_t child_count = ts_node_child_count(node);
    for (std::uint32_t i = 0; i < child_count; ++i) {
        total += count_named_recursive(ts_node_child(node, i));
    }
    return total;
}

} // namespace

void Ast::visit_named_nodes(const Visitor& visit) const {
    if (impl_ == nullptr || impl_->tree == nullptr) return;
    visit_recursive(ts_tree_root_node(impl_->tree), visit);
}

auto Ast::count_named_nodes() const noexcept -> std::size_t {
    if (impl_ == nullptr || impl_->tree == nullptr) return 0;
    return count_named_recursive(ts_tree_root_node(impl_->tree));
}

auto Ast::node_text(SourceRange range) const noexcept -> std::string_view {
    if (impl_ == nullptr) return {};
    const auto& s = impl_->source;
    if (range.start_byte >= s.size() || range.end_byte > s.size() ||
        range.end_byte < range.start_byte) {
        return {};
    }
    return std::string_view{s.data() + range.start_byte,
                            range.end_byte - range.start_byte};
}

} // namespace euxis::parse
