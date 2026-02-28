# SPDX-License-Identifier: AGPL-3.0-or-later

from __future__ import annotations

from adapters.registry import build_adapters
from adapters.sdk import AdapterConfig, require


def test_require_and_adapter_config() -> None:
    cfg = AdapterConfig(enabled=True, mode="socket")
    assert cfg.enabled is True
    assert cfg.mode == "socket"
    assert require({"token": "abc"}, "token") == "abc"


def test_require_missing_raises() -> None:
    try:
        require({}, "token")
        raise AssertionError("expected ValueError")
    except ValueError as exc:
        assert "Missing required config: token" in str(exc)


def test_build_adapters_all_channels_enabled(monkeypatch) -> None:
    created = {}

    class FakeSlack:
        def __init__(self, config, on_message=None):
            created["slack"] = (config, on_message)

    class FakeTelegram:
        def __init__(self, config, on_message=None):
            created["telegram"] = (config, on_message)

    class FakeDiscord:
        def __init__(self, config, on_message=None):
            created["discord"] = (config, on_message)

    class FakeWhatsApp:
        def __init__(self, config, on_message=None):
            created["whatsapp"] = (config, on_message)

    monkeypatch.setattr("adapters.registry.SlackAdapter", FakeSlack)
    monkeypatch.setattr("adapters.registry.TelegramAdapter", FakeTelegram)
    monkeypatch.setattr("adapters.registry.DiscordAdapter", FakeDiscord)
    monkeypatch.setattr("adapters.registry.WhatsAppAdapter", FakeWhatsApp)

    def on_message(*_args, **_kwargs):
        return None

    config = {
        "gateway": {
            "channels": {
                "slack": {"enabled": True, "token": "st", "app_token": "at", "mode": "socket"},
                "telegram": {
                    "enabled": True,
                    "token": "tt",
                    "mode": "webhook",
                    "webhook_url": "https://x",
                    "poll_timeout": 3,
                    "poll_interval": 0.5,
                },
                "discord": {"enabled": True, "token": "dt"},
                "whatsapp": {"enabled": True, "token": "wt", "phone_number_id": "pn", "verify_token": "vt"},
            }
        }
    }
    adapters = build_adapters(config, on_message=on_message)
    assert set(adapters.keys()) == {"slack", "telegram", "discord", "whatsapp"}
    assert created["slack"][0].token == "st"
    assert created["telegram"][0].poll_timeout == 3
    assert created["discord"][0].enabled is True
    assert created["whatsapp"][0].phone_number_id == "pn"


def test_build_adapters_none_enabled() -> None:
    adapters = build_adapters({"gateway": {"channels": {}}})
    assert adapters == {}

