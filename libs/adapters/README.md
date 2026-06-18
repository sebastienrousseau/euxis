<!-- SPDX-License-Identifier: AGPL-3.0-only -->

<p align="center">
  <img src="https://cloudcdn.pro/euxis/v1/logos/euxis.svg" alt="Euxis logo" width="128" />
</p>

<h1 align="center">euxis::adapters</h1>

<p align="center">
  Channel adapters for euxis: Slack, Discord, Telegram, WhatsApp.
  Per-channel adapter classes around the C++23 <code>Adapter</code> concept.
</p>

<p align="center">
  <a href="https://github.com/sebastienrousseau/euxis/actions/workflows/cpp.yml"><img src="https://img.shields.io/github/actions/workflow/status/sebastienrousseau/euxis/cpp.yml?style=for-the-badge&logo=github" alt="Build" /></a>
  <img src="https://img.shields.io/badge/C%2B%2B-23-blue?style=for-the-badge&logo=cplusplus" alt="C++23" />
  <a href="../../LICENSE"><img src="https://img.shields.io/badge/license-AGPL--3.0-blue?style=for-the-badge" alt="License" /></a>
</p>

---

## Contents

- [Install](#install)
- [Quick start](#quick-start)
- [Public surface](#public-surface)
- [Examples](#examples)
- [License](#license)

---

## Install

```cmake
add_subdirectory(libs/adapters)
target_link_libraries(my_app PRIVATE euxis-adapters-cpp)
```

## Quick start

```cpp
#include <iostream>

#include "euxis/adapters/slack.hpp"

int main() {
    using namespace euxis::adapters;

    SlackAdapterConfig cfg;
    cfg.bot_token        = std::getenv("SLACK_BOT_TOKEN");
    cfg.signing_secret   = std::getenv("SLACK_SIGNING_SECRET");
    cfg.default_channel  = "#scan-results";

    SlackAdapter slack{cfg};

    const auto rc = slack.post_message("#scan-results", "Scan complete.");
    std::cout << "posted: " << rc.has_value() << '\n';
}
```

## Public surface

| Header | What it owns |
|---|---|
| `adapter.hpp` | The `Adapter` C++20 concept (`send`, `receive`, `id`) |
| `types.hpp` | `AdapterConfig`, plus per-channel config structs |
| `slack.hpp` | `SlackAdapter` |
| `discord.hpp` | `DiscordAdapter` |
| `telegram.hpp` | `TelegramAdapter` |
| `whatsapp.hpp` | `WhatsAppAdapter` |
| `registry.hpp` | `AdapterRegistry` — registers + dispatches across multiple adapters by id |

Each adapter type satisfies the `Adapter` concept. Consumers can write generic code over `auto a -> Adapter` without naming the specific channel.

## Examples

### Generic dispatch via the `Adapter` concept

```cpp
#include "euxis/adapters/adapter.hpp"

template <euxis::adapters::Adapter A>
auto broadcast(A& adapter, std::string_view target, std::string_view text) {
    return adapter.send(target, text);
}
```

## License

AGPL-3.0-only. See [`LICENSE`](../../LICENSE).
