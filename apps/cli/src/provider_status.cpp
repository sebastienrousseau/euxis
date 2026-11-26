// Copyright (c) 2026 The Euxis Authors
// SPDX-License-Identifier: AGPL-3.0-only
#include "euxis/cli/provider_status.hpp"
#include "euxis/cli/process.hpp"
#include "euxis/cli/terminal.hpp"

#include <cstdlib>
#include <ostream>

namespace euxis::cli {
namespace term = terminal;

namespace {

auto has_env_key(const std::string& provider) -> bool {
    if (provider == "openai") return std::getenv("OPENAI_API_KEY") != nullptr;
    if (provider == "gemini") return std::getenv("GEMINI_API_KEY") != nullptr;
    return false;
}

} // namespace

auto provider_auth_kind(AuthProfileStore& store, const std::string& provider) -> ProviderAuthKind {
    if (provider == "claude") {
        return Process::available("claude") ? ProviderAuthKind::CliSession : ProviderAuthKind::NotConfigured;
    }
    if (has_env_key(provider)) return ProviderAuthKind::ApiKey;
    auto auth = store.resolve_with_fallback(provider);
    if (auth && !auth->token.empty()) return ProviderAuthKind::OAuthSession;
    return Process::available(provider) ? ProviderAuthKind::CliSession : ProviderAuthKind::NotConfigured;
}

void print_provider_auth(std::ostream& out, const std::string& provider, ProviderAuthKind kind,
                         const std::string& reauth_cmd) {
    const std::string lead = "    ";
    switch (kind) {
    case ProviderAuthKind::ApiKey:
        out << lead << term::icon_ok() << " " << provider << " " << term::green("(Native API Key Active)") << "\n";
        return;
    case ProviderAuthKind::OAuthSession:
        out << lead << term::icon_ok() << " " << provider << " " << term::blue("(OAuth Session Active)") << "\n";
        return;
    case ProviderAuthKind::CliSession:
        out << lead << term::icon_ok() << " " << provider << " " << term::dim("(Binary found, CLI auth preserved)")
            << "\n";
        return;
    case ProviderAuthKind::NotConfigured:
        out << lead << term::icon_warn() << " " << provider << " " << term::dim("(Not Configured -> ")
            << term::yellow(reauth_cmd) << term::dim(")") << "\n";
        return;
    }
}

} // namespace euxis::cli
