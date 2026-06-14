// Fixture used to exercise has_errors(). The unterminated function
// body forces tree-sitter to insert at least one ERROR node into the
// recovered AST.

int broken(void) {
    return 1
    // missing semicolon and closing brace
