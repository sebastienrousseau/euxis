import json
import os
import subprocess
import sys
from pathlib import Path
from unittest.mock import MagicMock, patch

import pytest

from cli import _run_interactive, _run_artifact_only, main, __version__

def test_version():
    assert __version__ == "v0.0.2"

@patch("cli.subprocess.run")
def test_run_interactive(mock_run):
    mock_run.return_value.returncode = 0
    bash_script = Path("/mock/euxis.sh")
    args = ["--interactive"]
    
    code = _run_interactive(bash_script, args)
    assert code == 0
    mock_run.assert_called_once_with(
        ["/mock/euxis.sh", "--interactive"],
        env=os.environ
    )

@patch("cli.Console")
@patch("cli.subprocess.Popen")
@patch("cli.json.loads")
def test_run_artifact_only_success_json(mock_loads, mock_popen, mock_console):
    # Setup mock process
    mock_process = MagicMock()
    mock_process.returncode = 0
    
    # Needs to match exactly what a JSON payload string looks like
    json_string = '{"artifact": "data"}'
    mock_loads.return_value = {"artifact": "data"}
    
    mock_process.stdout.readline.side_effect = [
        "",  # Trigger 45->44 condition (if line logic evaluates False)
        json_string,
        ""
    ]
    mock_popen.return_value = mock_process

    console_instance = MagicMock()
    mock_console.return_value = console_instance

    bash_script = Path("/mock/euxis.sh")
    args = ["--artifact-only", "agent2", "task"]
    
    code = _run_artifact_only(bash_script, args)
    
    assert code == 0
    mock_popen.assert_called_once()
    assert mock_popen.call_args[0][0] == ["/mock/euxis.sh", "agent2", "task"]
    
    # Verify JSON block printed
    console_instance.print.assert_called_once()
    syntax_call = console_instance.print.call_args[0][0]
    
    # json.dumps formatting uses 2 spaces
    expected_formatted = '{\n  "artifact": "data"\n}'
    assert syntax_call.code == expected_formatted
    assert getattr(syntax_call.lexer, "name", getattr(syntax_call, "lexer_name", "")) in ("JSON", "json")

@patch("cli.Console")
@patch("cli.subprocess.Popen")
def test_run_artifact_only_success_markdown(mock_popen, mock_console):
    mock_process = MagicMock()
    mock_process.returncode = 0
    mock_process.stdout.readline.side_effect = [
        "Plan output:\n",
        "```markdown\n",
        "# Header\n",
        "```\n",
        ""
    ]
    mock_popen.return_value = mock_process

    console_instance = MagicMock()
    mock_console.return_value = console_instance

    code = _run_artifact_only(Path("/mock/euxis.sh"), [])
    
    assert code == 0
    console_instance.print.assert_called_once()
    markdown_call = console_instance.print.call_args[0][0]
    # Check that it extracted the block
    assert "Header" in markdown_call.markup

@patch("cli.Console")
@patch("cli.subprocess.Popen")
def test_run_artifact_only_success_raw_markdown(mock_popen, mock_console):
    mock_process = MagicMock()
    mock_process.returncode = 0
    mock_process.stdout.readline.side_effect = ["# Just Header\n", ""]
    mock_popen.return_value = mock_process

    console_instance = MagicMock()
    mock_console.return_value = console_instance

    code = _run_artifact_only(Path("/mock/euxis.sh"), [])
    
    assert code == 0
    console_instance.print.assert_called_once()
    markdown_call = console_instance.print.call_args[0][0]
    # Check raw fallback
    assert markdown_call.markup == "# Just Header"

@patch("cli.Console")
@patch("cli.subprocess.Popen")
def test_run_artifact_only_failure(mock_popen, mock_console):
    mock_process = MagicMock()
    mock_process.returncode = 1
    mock_process.stdout.readline.side_effect = [""]
    mock_process.stderr.read.return_value = "bash error trace"
    mock_popen.return_value = mock_process

    console_instance = MagicMock()
    mock_console.return_value = console_instance

    code = _run_artifact_only(Path("/mock/euxis.sh"), [])
    
    assert code == 1
    # Error message + Syntax block
    assert console_instance.print.call_count == 2
    assert "Execution Failed" in console_instance.print.call_args_list[0][0][0]
    
    syntax_call = console_instance.print.call_args_list[1][0][0]
    assert syntax_call.code == "bash error trace"
    assert syntax_call.lexer.name == "Bash"

@patch("cli.Console")
@patch("cli.subprocess.Popen")
def test_run_artifact_only_failure_no_stderr(mock_popen, mock_console):
    # Tests branch line 53->55 (if error_out is empty/None)
    mock_process = MagicMock()
    mock_process.returncode = 1
    mock_process.stdout.readline.side_effect = [""]
    mock_process.stderr.read.return_value = ""
    mock_popen.return_value = mock_process

    console_instance = MagicMock()
    mock_console.return_value = console_instance

    code = _run_artifact_only(Path("/mock/euxis.sh"), [])
    
    assert code == 1
    # Only the first line prints, no Syntax block
    console_instance.print.assert_called_once()
    assert "Execution Failed" in console_instance.print.call_args[0][0]

@patch("cli.Console")
@patch("cli.subprocess.Popen")
def test_run_artifact_only_success_markdown_no_blocks(mock_popen, mock_console):
    # Tests branch 71->74 (if len(parts) <= 1 inside a markdown string payload)
    mock_process = MagicMock()
    mock_process.returncode = 0
    # Must contain "```markdown" to enter block 68, but no second part to fail block 71
    mock_process.stdout.readline.side_effect = ["```markdown", ""]
    mock_popen.return_value = mock_process

    console_instance = MagicMock()
    mock_console.return_value = console_instance

    code = _run_artifact_only(Path("/mock/euxis.sh"), [])
    
    assert code == 0
    console_instance.print.assert_called_once()
    markdown_call = console_instance.print.call_args[0][0]
    # In `_run_artifact_only` line 67, if `len(parts) <= 1`, then `markdown_block`
    # remains exactly `raw_output`. Thus the markup check needs to match it.
    assert markdown_call.markup == ""

@patch("sys.argv", ["euxis"])
@patch("cli.print")
def test_main_script_not_found(mock_print, tmp_path):
    # Setup missing script
    os.environ["EUXIS_HOME"] = str(tmp_path)
    
    code = main()
    
    assert code == 1
    mock_print.assert_any_call("Please ensure EUXIS_HOME is set correctly.")

@patch("sys.argv", ["euxis", "agent"])
@patch("cli._run_interactive")
def test_main_interactive(mock_run, tmp_path):
    script_dir = tmp_path / "euxis-cli" / "bin"
    script_dir.mkdir(parents=True)
    script_file = script_dir / "euxis.sh"
    script_file.touch()
    
    os.environ["EUXIS_HOME"] = str(tmp_path)
    mock_run.return_value = 0
    
    code = main()
    
    assert code == 0
    mock_run.assert_called_once_with(script_file, ["agent"])

def test_main_artifact_only(tmp_path):
    script_dir = tmp_path / "euxis-cli" / "bin"
    script_dir.mkdir(parents=True)
    script_file = script_dir / "euxis.sh"
    script_file.touch()
    
    os.environ["EUXIS_HOME"] = str(tmp_path)
    
    with patch("sys.argv", ["euxis", "--artifact-only", "agent2", "task"]):
        with patch("cli._run_artifact_only", return_value=0) as mock_run_artifact:
            code = main()
            assert code == 0
            mock_run_artifact.assert_called_once_with(script_file, ["--artifact-only", "agent2", "task"])
