# SPDX-License-Identifier: MIT
# Copyright (c) 2024-2026 Euxis Contributors

"""Euxis CLI - Command-line interface for the Euxis Agent Framework."""

import os
import subprocess
import sys
from pathlib import Path

__version__ = "0.1.0"


def main() -> int:
    """Main entry point for the euxis CLI.

    Delegates to the bash implementation for full functionality.
    """
    # Find the bash script
    euxis_home = os.environ.get("EUXIS_HOME", str(Path.home() / ".euxis"))
    bash_script = Path(euxis_home) / "euxis-cli" / "bin" / "euxis.sh"

    if bash_script.exists():
        # Execute the bash script with all arguments
        result = subprocess.run(
            [str(bash_script)] + sys.argv[1:],
            env=os.environ,
        )
        return result.returncode
    else:
        # Fallback if bash script not found
        print(f"Error: Euxis bash script not found at {bash_script}")
        print("Please ensure EUXIS_HOME is set correctly.")
        return 1


__all__ = ["__version__", "main"]
