# SPDX-License-Identifier: MIT
# Copyright (c) 2024-2026 Euxis Contributors

"""Euxis CLI - Command-line interface for the Euxis Agent Framework."""

import os
import subprocess
import sys
import json
from pathlib import Path
from rich.console import Console
from rich.markdown import Markdown
from rich.status import Status
from rich.syntax import Syntax

__version__ = "0.1.0"

def _run_interactive(bash_script: Path, args: list[str]) -> int:
    """Run script in fully interactive terminal passthrough."""
    result = subprocess.run(
        [str(bash_script)] + args,
        env=os.environ,
    )
    return result.returncode

def _run_artifact_only(bash_script: Path, args: list[str]) -> int:
    """Run script capturing stdout and rendering exclusively final artifacts via rich."""
    console = Console()
    
    # Remove flag before passing deeper
    clean_args = [arg for arg in args if arg != "--artifact-only"]
    
    with Status("[cyan]Euxis Agent mesh compiling context...", spinner="dots") as status:
        process = subprocess.Popen(
            [str(bash_script)] + clean_args,
            env=os.environ,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1,
        )
        
        output_lines = []
        for line in iter(process.stdout.readline, ""):
            if line:
                output_lines.append(line)
        
        process.wait()

    if process.returncode != 0:
        error_out = process.stderr.read()
        console.print(f"[bold red]Execution Failed (Code {process.returncode})[/bold red]")
        if error_out:
             console.print(Syntax(error_out, "bash", theme="monokai"))
        return process.returncode

    # Process and render the buffered payload artifact
    raw_output = "".join(output_lines).strip()
    
    try:
        # Check if the artifact is JSON
        data = json.loads(raw_output)
        formatted_json = json.dumps(data, indent=2)
        console.print(Syntax(formatted_json, "json", theme="monokai"))
    except json.JSONDecodeError:
        # Not JSON, fallback to Markdown extraction
        markdown_block = raw_output
        if "```markdown" in raw_output:
            # Attempt to extract explicit markdown codeblocks
            parts = raw_output.split("```markdown")
            if len(parts) > 1:
               markdown_block = parts[1].split("```")[0].strip()
        
        console.print(Markdown(markdown_block))

    return 0

def main() -> int:
    """Main entry point for the euxis CLI.
    Delegates to the bash implementation for full functionality.
    """
    euxis_home = os.environ.get("EUXIS_HOME", str(Path.home() / ".euxis"))
    bash_script = Path(euxis_home) / "euxis-cli" / "bin" / "euxis.sh"

    if not bash_script.exists():
        print(f"Error: Euxis bash script not found at {bash_script}")
        print("Please ensure EUXIS_HOME is set correctly.")
        return 1

    if "--artifact-only" in sys.argv:
        return _run_artifact_only(bash_script, sys.argv[1:])
    else:
        return _run_interactive(bash_script, sys.argv[1:])

__all__ = ["__version__", "main"]
