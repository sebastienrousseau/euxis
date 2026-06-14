#include "euxis/cpg/types.hpp"

namespace euxis::cpg {

auto node_kind_str(NodeKind k) noexcept -> const char* {
    switch (k) {
        case NodeKind::Unknown:         return "Unknown";
        case NodeKind::TranslationUnit: return "TranslationUnit";
        case NodeKind::FunctionDef:     return "FunctionDef";
        case NodeKind::Parameter:       return "Parameter";
        case NodeKind::Block:           return "Block";
        case NodeKind::Call:            return "Call";
        case NodeKind::Identifier:      return "Identifier";
        case NodeKind::Literal:         return "Literal";
        case NodeKind::Assignment:      return "Assignment";
        case NodeKind::Return:          return "Return";
        case NodeKind::If:              return "If";
        case NodeKind::Loop:            return "Loop";
        case NodeKind::Declaration:     return "Declaration";
        case NodeKind::PreprocInclude:  return "PreprocInclude";
        case NodeKind::Comment:         return "Comment";
    }
    return "Unknown";
}

auto edge_kind_str(EdgeKind k) noexcept -> const char* {
    switch (k) {
        case EdgeKind::AstChild: return "AstChild";
        case EdgeKind::Cfg:      return "Cfg";
        case EdgeKind::Ddg:      return "Ddg";
        case EdgeKind::Call:     return "Call";
    }
    return "AstChild";
}

namespace {

// Cross-language vocabulary. Tree-sitter grammars share many of
// these names verbatim (e.g. "identifier", "block"), so the shared
// section handles every grammar before per-language overrides
// pick up the rest.
auto classify_common(std::string_view k) noexcept -> NodeKind {
    if (k == "identifier")                          return NodeKind::Identifier;
    if (k == "call_expression" || k == "call")      return NodeKind::Call;
    if (k == "block" || k == "compound_statement")  return NodeKind::Block;
    if (k == "return_statement" || k == "return")   return NodeKind::Return;
    if (k == "if_statement" || k == "if_expression") return NodeKind::If;
    if (k == "for_statement"   || k == "for_expression" ||
        k == "while_statement" || k == "while_expression" ||
        k == "do_statement")                        return NodeKind::Loop;
    if (k == "assignment_expression" || k == "assignment") return NodeKind::Assignment;
    if (k == "number_literal" || k == "string_literal" ||
        k == "boolean_literal" || k == "true" || k == "false" ||
        k == "integer_literal" || k == "float_literal" ||
        k == "char_literal"    || k == "raw_string_literal" ||
        k == "string"          || k == "number" ||
        k == "true_literal"    || k == "false_literal" ||
        k == "nil"             || k == "none" || k == "null")
        return NodeKind::Literal;
    if (k == "comment" || k == "line_comment" || k == "block_comment")
        return NodeKind::Comment;
    return NodeKind::Unknown;
}

} // namespace

auto classify(euxis::parse::Language lang,
              std::string_view raw_kind) noexcept -> NodeKind {
    // Translation-unit-ish roots first — every grammar names this
    // node differently.
    if (raw_kind == "translation_unit" ||  // C, C++
        raw_kind == "source_file" ||        // Rust, Go
        raw_kind == "module" ||             // Python
        raw_kind == "program" ||            // JavaScript, TypeScript
        raw_kind == "compilation_unit") {   // Java
        return NodeKind::TranslationUnit;
    }

    // Per-language fast paths. Each block handles only the kinds
    // whose names differ from the cross-language vocabulary or
    // whose classification is grammar-specific.
    switch (lang) {
        case euxis::parse::Language::C:
        case euxis::parse::Language::Cpp:
            if (raw_kind == "function_definition" ||
                raw_kind == "function_declarator")  return NodeKind::FunctionDef;
            if (raw_kind == "parameter_declaration") return NodeKind::Parameter;
            if (raw_kind == "preproc_include")       return NodeKind::PreprocInclude;
            if (raw_kind == "declaration")           return NodeKind::Declaration;
            break;
        case euxis::parse::Language::Rust:
            if (raw_kind == "function_item")         return NodeKind::FunctionDef;
            if (raw_kind == "parameter")             return NodeKind::Parameter;
            if (raw_kind == "use_declaration")       return NodeKind::PreprocInclude;
            if (raw_kind == "let_declaration" ||
                raw_kind == "const_item" ||
                raw_kind == "static_item")           return NodeKind::Declaration;
            break;
        case euxis::parse::Language::Go:
            if (raw_kind == "function_declaration" ||
                raw_kind == "method_declaration")    return NodeKind::FunctionDef;
            if (raw_kind == "parameter_declaration") return NodeKind::Parameter;
            if (raw_kind == "import_declaration")    return NodeKind::PreprocInclude;
            if (raw_kind == "var_declaration" ||
                raw_kind == "const_declaration" ||
                raw_kind == "short_var_declaration") return NodeKind::Declaration;
            break;
        case euxis::parse::Language::Python:
            if (raw_kind == "function_definition")   return NodeKind::FunctionDef;
            if (raw_kind == "parameters" ||
                raw_kind == "default_parameter" ||
                raw_kind == "typed_parameter")       return NodeKind::Parameter;
            if (raw_kind == "import_statement" ||
                raw_kind == "import_from_statement") return NodeKind::PreprocInclude;
            if (raw_kind == "assignment")            return NodeKind::Assignment;
            break;
        case euxis::parse::Language::JavaScript:
        case euxis::parse::Language::TypeScript:
            if (raw_kind == "function_declaration" ||
                raw_kind == "arrow_function" ||
                raw_kind == "method_definition" ||
                raw_kind == "function_expression")   return NodeKind::FunctionDef;
            if (raw_kind == "formal_parameters" ||
                raw_kind == "required_parameter" ||
                raw_kind == "optional_parameter" ||
                raw_kind == "rest_pattern")          return NodeKind::Parameter;
            if (raw_kind == "import_statement")      return NodeKind::PreprocInclude;
            if (raw_kind == "variable_declaration" ||
                raw_kind == "lexical_declaration")   return NodeKind::Declaration;
            if (raw_kind == "for_in_statement" ||
                raw_kind == "for_of_statement")      return NodeKind::Loop;
            break;
        case euxis::parse::Language::Java:
            if (raw_kind == "method_declaration" ||
                raw_kind == "constructor_declaration") return NodeKind::FunctionDef;
            if (raw_kind == "formal_parameter")      return NodeKind::Parameter;
            if (raw_kind == "import_declaration")    return NodeKind::PreprocInclude;
            if (raw_kind == "local_variable_declaration" ||
                raw_kind == "field_declaration")     return NodeKind::Declaration;
            if (raw_kind == "enhanced_for_statement") return NodeKind::Loop;
            break;
    }

    return classify_common(raw_kind);
}

} // namespace euxis::cpg
