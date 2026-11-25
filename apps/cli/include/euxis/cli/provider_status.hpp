// Copyright (c) 2026 The Euxis Authors
// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
/// @file
/// @brief How each provider will authenticate, for the pre-flight probe.
///
/// The claude provider always runs through the user's Claude Code CLI
/// login (see ProviderExecutor), so it is ready exactly when the `claude`
/// binary is on PATH; an Anthropic API key is never reported, because
/// euxis never uses one.

#include "euxis/cli/auth_profile_store.hpp"

#include <iosfwd>
#include <string>

namespace euxis::cli {

enum class ProviderAuthKind { ApiKey, OAuthSession, CliSession, NotConfigured };

/// Classify how @p provider will authenticate on this machine.
[[nodiscard]] auto provider_auth_kind(AuthProfileStore& store, const std::string& provider)
    -> ProviderAuthKind;

/// Print one probe line for @p provider; @p reauth_cmd is shown when it is not configured.
void print_provider_auth(std::ostream& out, const std::string& provider, ProviderAuthKind kind,
                         const std::string& reauth_cmd);

} // namespace euxis::cli
