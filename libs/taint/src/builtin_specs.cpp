#include "euxis/taint/builtin_specs.hpp"

namespace euxis::taint {

namespace {

using L = euxis::parse::Language;
using euxis::security::OwaspCategory;
using euxis::security::Severity;

} // namespace

auto builtin_specs() -> TaintSpec {
    TaintSpec t;
    t.name = "euxis-builtin";

    // ---- Sources -----------------------------------------------------------
    // C / C++ — process input + environment + file reads.
    t.sources.push_back({"c.argv",   {L::C, L::Cpp}, "argv",   "Process argv"});
    t.sources.push_back({"c.getenv", {L::C, L::Cpp}, "getenv", "Environment variable read"});
    t.sources.push_back({"c.scanf",  {L::C, L::Cpp}, "scanf",  "Untrusted user input via scanf family"});
    t.sources.push_back({"c.gets",   {L::C, L::Cpp}, "gets",   "Unsafe gets() — deprecated"});

    // Rust — env, std::io, command-line args.
    t.sources.push_back({"rs.env_var", {L::Rust}, "env::var",   "std::env::var"});
    t.sources.push_back({"rs.args",    {L::Rust}, "env::args",  "Command-line args"});
    t.sources.push_back({"rs.read",    {L::Rust}, "read_line",  "User input via read_line"});

    // Go — env + flag + http request.
    t.sources.push_back({"go.getenv",      {L::Go}, "os.Getenv",            "Environment variable"});
    t.sources.push_back({"go.r_form",      {L::Go}, "r.FormValue",          "http.Request form value"});
    t.sources.push_back({"go.flag_string", {L::Go}, "flag.String",          "Command-line flag"});

    // Python — input, environ, request args, sys.argv.
    t.sources.push_back({"py.input",      {L::Python}, "input(",       "Python input()"});
    t.sources.push_back({"py.environ",    {L::Python}, "os.environ",   "Environment variable"});
    t.sources.push_back({"py.argv",       {L::Python}, "sys.argv",     "Command-line args"});
    t.sources.push_back({"py.request",    {L::Python}, "request.",     "Flask/Django request object"});

    // JavaScript / TypeScript — req body, query string, location, document.
    t.sources.push_back({"js.req_body",   {L::JavaScript, L::TypeScript}, "req.body",         "Express request body"});
    t.sources.push_back({"js.req_query",  {L::JavaScript, L::TypeScript}, "req.query",        "Express request query"});
    t.sources.push_back({"js.req_params", {L::JavaScript, L::TypeScript}, "req.params",       "Express request params"});
    t.sources.push_back({"js.location",   {L::JavaScript, L::TypeScript}, "window.location",  "DOM location"});

    // Java — HttpServletRequest, System.getenv, args.
    t.sources.push_back({"java.req_param", {L::Java}, "getParameter",    "HttpServletRequest parameter"});
    t.sources.push_back({"java.getenv",    {L::Java}, "System.getenv",   "Environment variable"});

    // ---- Sinks ------------------------------------------------------------
    // C / C++.
    t.sinks.push_back({.id = "c.system",   .languages = {L::C, L::Cpp},
                       .pattern = "system",  .description = "Shell command via system()",
                       .cwe = "CWE-78", .owasp = OwaspCategory::A03_SupplyChainFailures,
                       .severity = Severity::High});
    t.sinks.push_back({.id = "c.popen",    .languages = {L::C, L::Cpp},
                       .pattern = "popen",   .description = "Shell command via popen()",
                       .cwe = "CWE-78", .owasp = OwaspCategory::A03_SupplyChainFailures,
                       .severity = Severity::High});
    t.sinks.push_back({.id = "c.exec",     .languages = {L::C, L::Cpp},
                       .pattern = "execl",   .description = "Process exec call",
                       .cwe = "CWE-78", .owasp = OwaspCategory::A03_SupplyChainFailures,
                       .severity = Severity::High});
    t.sinks.push_back({.id = "c.sprintf",  .languages = {L::C, L::Cpp},
                       .pattern = "sprintf", .description = "Unsafe sprintf — buffer overflow risk",
                       .cwe = "CWE-120", .owasp = std::nullopt, .severity = Severity::High});

    // Rust.
    t.sinks.push_back({.id = "rs.command", .languages = {L::Rust},
                       .pattern = "Command::new", .description = "Process spawn",
                       .cwe = "CWE-78", .owasp = std::nullopt, .severity = Severity::High});
    t.sinks.push_back({.id = "rs.unsafe",  .languages = {L::Rust},
                       .pattern = "unsafe", .description = "Unsafe block — manual review required",
                       .cwe = "CWE-1357", .owasp = std::nullopt, .severity = Severity::Medium});

    // Go.
    t.sinks.push_back({.id = "go.exec",       .languages = {L::Go},
                       .pattern = "exec.Command", .description = "Process spawn",
                       .cwe = "CWE-78", .owasp = std::nullopt, .severity = Severity::High});
    t.sinks.push_back({.id = "go.sql_exec",   .languages = {L::Go},
                       .pattern = ".Exec(", .description = "Raw SQL exec",
                       .cwe = "CWE-89", .owasp = std::nullopt, .severity = Severity::High});

    // Python.
    t.sinks.push_back({.id = "py.eval",       .languages = {L::Python},
                       .pattern = "eval(", .description = "Python eval()",
                       .cwe = "CWE-94", .owasp = OwaspCategory::A03_SupplyChainFailures,
                       .severity = Severity::Critical});
    t.sinks.push_back({.id = "py.exec",       .languages = {L::Python},
                       .pattern = "exec(", .description = "Python exec()",
                       .cwe = "CWE-94", .owasp = std::nullopt, .severity = Severity::Critical});
    t.sinks.push_back({.id = "py.subprocess", .languages = {L::Python},
                       .pattern = "subprocess.", .description = "Subprocess call",
                       .cwe = "CWE-78", .owasp = std::nullopt, .severity = Severity::High});
    t.sinks.push_back({.id = "py.os_system",  .languages = {L::Python},
                       .pattern = "os.system", .description = "Shell command",
                       .cwe = "CWE-78", .owasp = std::nullopt, .severity = Severity::High});
    t.sinks.push_back({.id = "py.pickle",     .languages = {L::Python},
                       .pattern = "pickle.loads", .description = "Insecure deserialization",
                       .cwe = "CWE-502", .owasp = OwaspCategory::A08_DataIntegrityFailures,
                       .severity = Severity::Critical});

    // JavaScript / TypeScript.
    t.sinks.push_back({.id = "js.eval",       .languages = {L::JavaScript, L::TypeScript},
                       .pattern = "eval(", .description = "JS eval()",
                       .cwe = "CWE-94", .owasp = std::nullopt, .severity = Severity::Critical});
    t.sinks.push_back({.id = "js.innerhtml",  .languages = {L::JavaScript, L::TypeScript},
                       .pattern = "innerHTML", .description = "DOM XSS sink",
                       .cwe = "CWE-79", .owasp = std::nullopt, .severity = Severity::High});
    t.sinks.push_back({.id = "js.document_write", .languages = {L::JavaScript, L::TypeScript},
                       .pattern = "document.write", .description = "document.write XSS sink",
                       .cwe = "CWE-79", .owasp = std::nullopt, .severity = Severity::High});
    t.sinks.push_back({.id = "js.exec_sync",  .languages = {L::JavaScript, L::TypeScript},
                       .pattern = "execSync", .description = "child_process.execSync",
                       .cwe = "CWE-78", .owasp = std::nullopt, .severity = Severity::High});

    // Java.
    t.sinks.push_back({.id = "java.runtime_exec", .languages = {L::Java},
                       .pattern = "Runtime.getRuntime().exec", .description = "Runtime exec",
                       .cwe = "CWE-78", .owasp = std::nullopt, .severity = Severity::High});
    t.sinks.push_back({.id = "java.stmt_exec", .languages = {L::Java},
                       .pattern = "Statement", .description = "JDBC Statement (use PreparedStatement)",
                       .cwe = "CWE-89", .owasp = std::nullopt, .severity = Severity::High});

    // ---- Sanitizers -------------------------------------------------------
    // Cross-language: HTML escape, URL encode, parameterised SQL.
    t.sanitizers.push_back({"html.escape_py",     {L::Python},                       "html.escape",         "Python HTML escape"});
    t.sanitizers.push_back({"html.escape_js",     {L::JavaScript, L::TypeScript},    "encodeURIComponent",  "URI encoder"});
    t.sanitizers.push_back({"sql.prepared_java",  {L::Java},                         "PreparedStatement",   "Parameterised SQL"});
    t.sanitizers.push_back({"shlex_quote",        {L::Python},                       "shlex.quote",         "Shell argument quoting"});
    t.sanitizers.push_back({"go.html_escape",     {L::Go},                           "html.EscapeString",   "HTML escape"});
    t.sanitizers.push_back({"rs.shell_words",     {L::Rust},                         "shell_words::quote",  "Shell argument quoting"});

    return t;
}

} // namespace euxis::taint
