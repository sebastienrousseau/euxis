#!/usr/bin/env python3
"""
Codebase Quality & Debt Enforcement Script
Limits the number of `# pragma: no cover` and `TODO`/`FIXME` instances.
"""

import os
import sys

# Current debt baselines (must only go down, never up)
MAX_PRAGMAS = 183
MAX_TODOS = 13

search_dirs = [
    "euxis-adapters", "euxis-cli", "euxis-core", "euxis-crypto-lib",
    "euxis-gateway", "euxis-metrics", "euxis-runtime", "euxis-security", "euxis-tui"
]

def check_debt():
    pragma_count = 0
    todo_count = 0
    repo_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    
    for d in search_dirs:
        target_dir = os.path.join(repo_root, d, "src")
        if not os.path.exists(target_dir):
            # Also check if the project is flat (e.g. euxis-cli)
            target_dir = os.path.join(repo_root, d)
            if not os.path.exists(target_dir):
                continue
            
        for root, _, files in os.walk(target_dir):
            if "tests" in root or ".venv" in root or "egg-info" in root:
                continue
            
            for file in files:
                if not file.endswith(".py") and not file.endswith(".sh"):
                    continue
                
                path = os.path.join(root, file)
                try:
                    with open(path, "r", encoding="utf-8") as f:
                        lines = f.readlines()
                        for line in lines:
                            if "# pragma: no cover" in line:
                                pragma_count += 1
                            if "TODO" in line or "FIXME" in line:
                                todo_count += 1
                except Exception:
                    pass

    print(f"Current # pragma: no cover count: {pragma_count} (Limit: {MAX_PRAGMAS})")
    print(f"Current TODO/FIXME count: {todo_count} (Limit: {MAX_TODOS})")
    
    failed = False
    if pragma_count > MAX_PRAGMAS:
        print(f"❌ ERROR: Pragma no cover count ({pragma_count}) exceeds limit ({MAX_PRAGMAS}). Dark logic cannot increase.")
        failed = True
        
    if todo_count > MAX_TODOS:
        print(f"❌ ERROR: TODO/FIXME count ({todo_count}) exceeds limit ({MAX_TODOS}). Outstanding technical debt cannot increase.")
        failed = True
        
    if failed:
        sys.exit(1)
    else:
        print("✅ Debt limits are within boundaries. Ensure counts are driven towards 0.")
        sys.exit(0)

if __name__ == "__main__":
    check_debt()
