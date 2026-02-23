import types

from shared import adapter_loader


def test_build_adapters_import_error(monkeypatch):
    def boom(_name):
        raise ImportError("missing")

    monkeypatch.setattr(adapter_loader.importlib, "import_module", boom)
    assert adapter_loader.build_adapters() == {}


def test_build_adapters_new_signature(monkeypatch):
    def build_adapters(config, on_message=None):
        return {"ok": config, "on": on_message}

    module = types.SimpleNamespace(build_adapters=build_adapters)
    monkeypatch.setattr(adapter_loader.importlib, "import_module", lambda _name: module)
    result = adapter_loader.build_adapters({"gateway": {}}, on_message="handler")
    assert result["ok"] == {"gateway": {}}
    assert result["on"] == "handler"


def test_build_adapters_legacy_signature(monkeypatch):
    def build_adapters():
        return {"legacy": True}

    module = types.SimpleNamespace(build_adapters=build_adapters)
    monkeypatch.setattr(adapter_loader.importlib, "import_module", lambda _name: module)
    result = adapter_loader.build_adapters({"gateway": {}}, on_message="handler")
    assert result == {"legacy": True}
