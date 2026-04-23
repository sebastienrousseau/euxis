# euxis-adapters-cpp

C++23 external channel adapters for omnichannel messaging.

## Overview

Provides Slack, Telegram, Discord, and WhatsApp adapters via cpp-httplib. Each adapter implements the `Adapter` C++20 concept (connect, receive, send, ack, disconnect). A factory registry builds adapters from gateway configuration.

## Key Types

- `Adapter` concept -- C++20 concept defining the adapter interface
- `SlackAdapter` -- Slack REST API adapter with thread-aware sessions
- `TelegramAdapter` -- Telegram Bot API adapter (webhook mode)
- `DiscordAdapter` -- Discord webhook-based adapter
- `WhatsAppAdapter` -- Meta Graph API adapter
- `AnyAdapter` -- `std::variant` of all adapter types
- Config structs per adapter

## Key Functions

- `build_adapters(config, on_message)` -- Factory function from gateway JSON config

## Dependencies

- `httplib::httplib`
- `nlohmann_json::nlohmann_json`
- `spdlog::spdlog`

## Build

Part of the euxis-cpp CMake project. Built via `euxis_add_library()` macro.
