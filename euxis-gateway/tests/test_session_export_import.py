import json
import sys
from pathlib import Path

import pytest

from gateway import session_export, session_import


def test_session_export_main(tmp_path: Path, monkeypatch, capsys):
    out_path = tmp_path / "sessions.json"

    def fake_sessions_dir():
        return tmp_path

    def fake_load_session(session_id: str):
        return [{"id": session_id}]

    (tmp_path / "sess_a.jsonl").write_text("{}\n", encoding="utf-8")
    monkeypatch.setattr(session_export, "sessions_dir", fake_sessions_dir)
    monkeypatch.setattr(session_export, "load_session_from_disk", fake_load_session)
    monkeypatch.setattr(session_export.sys, "argv", ["session_export.py", "--out", str(out_path)])

    result = session_export.main()
    assert result == 0
    data = json.loads(out_path.read_text(encoding="utf-8"))
    assert data["sess_a"][0]["id"] == "sess_a"
    assert "Exported 1 sessions" in capsys.readouterr().out


def test_session_import_main(tmp_path: Path, monkeypatch, capsys):
    payload = {"sess_a": [{"role": "user"}, {"role": "assistant"}]}
    in_path = tmp_path / "input.json"
    in_path.write_text(json.dumps(payload), encoding="utf-8")

    seen = []

    def fake_persist(session_id, entry):
        seen.append((session_id, entry))

    monkeypatch.setattr(session_import, "persist_message", fake_persist)
    monkeypatch.setattr(session_import.sys, "argv", ["session_import.py", "--in", str(in_path)])

    result = session_import.main()
    assert result == 0
    assert len(seen) == 2
    assert "Imported 2 messages" in capsys.readouterr().out


def test_session_import_invalid_format(tmp_path: Path, monkeypatch):
    in_path = tmp_path / "input.json"
    in_path.write_text("[]", encoding="utf-8")
    monkeypatch.setattr(session_import.sys, "argv", ["session_import.py", "--in", str(in_path)])
    with pytest.raises(SystemExit):
        session_import.main()


def test_session_import_skips_invalid_entries(tmp_path: Path, monkeypatch, capsys):
    payload = {"sess_a": ["bad"], "sess_b": [{"role": "user"}, "bad2"], "sess_c": "bad"}
    in_path = tmp_path / "input.json"
    in_path.write_text(json.dumps(payload), encoding="utf-8")
    seen = []

    def fake_persist(session_id, entry):
        seen.append((session_id, entry))

    monkeypatch.setattr(session_import, "persist_message", fake_persist)
    monkeypatch.setattr(session_import.sys, "argv", ["session_import.py", "--in", str(in_path)])
    result = session_import.main()
    assert result == 0
    assert len(seen) == 1
    assert "Imported 1 messages" in capsys.readouterr().out


def test_session_export_import_sys_path_insert(monkeypatch):
    import importlib.util

    repo_root = Path(__file__).resolve().parents[2]
    export_path = repo_root / "euxis-gateway" / "src" / "gateway" / "session_export.py"
    import_path = repo_root / "euxis-gateway" / "src" / "gateway" / "session_import.py"

    original_exists = Path.exists

    def fake_exists(self):
        if str(self).endswith("api/src") or str(self).endswith("packages/shared/src"):
            return True
        return original_exists(self)

    monkeypatch.setattr(Path, "exists", fake_exists)

    repo_root_str = str(repo_root)
    if repo_root_str in sys.path:
        sys.path.remove(repo_root_str)

    spec_a = importlib.util.spec_from_file_location("gateway.session_export_re", export_path)
    module_a = importlib.util.module_from_spec(spec_a)
    assert spec_a and spec_a.loader
    spec_a.loader.exec_module(module_a)
    assert repo_root_str in sys.path

    if repo_root_str in sys.path:
        sys.path.remove(repo_root_str)

    spec_b = importlib.util.spec_from_file_location("gateway.session_import_re", import_path)
    module_b = importlib.util.module_from_spec(spec_b)
    assert spec_b and spec_b.loader
    spec_b.loader.exec_module(module_b)
    assert repo_root_str in sys.path

def _load_module_from_path(name: str, path: Path):
    import importlib.util
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    assert spec and spec.loader
    spec.loader.exec_module(module)
    return module


def test_module_path_bootstrap(monkeypatch, tmp_path: Path):
    repo_root = Path(__file__).resolve().parents[1]
    export_path = repo_root / "src" / "gateway" / "session_export.py"
    import_path = repo_root / "src" / "gateway" / "session_import.py"

    original_exists = Path.exists

    def fake_exists(self):
        if str(self).endswith("api/src") or str(self).endswith("packages/shared/src"):
            return True
        return original_exists(self)

    monkeypatch.setattr(Path, "exists", fake_exists)

    if str(repo_root) in sys.path:
        sys.path.remove(str(repo_root))

    module_a = _load_module_from_path("gateway.session_export_boot", export_path)
    module_b = _load_module_from_path("gateway.session_import_boot", import_path)

    assert module_a is not None
    assert module_b is not None
