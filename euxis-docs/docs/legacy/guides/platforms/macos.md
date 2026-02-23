# 🍎 Running Euxis on macOS (The "Unix-ish" Guide)

macOS users often mistake "Unix" for "Linux." While Euxis supports macOS, there are structural differences between standard Linux utilities and the default macOS BSD toolchain.

## GNU vs. BSD Tools

Many Euxis Agent Playbooks relying on internal shell scripts assume a standard GNU Linux environment.

macOS natively ships with **BSD** generic utilities.
* The macOS `sed` requires an empty string for in-place edits (`sed -i ''`).
* The macOS `grep` lacks the Perl-Compatible Regular Expression (`-P`) flag implicitly.

**Recommendation:** Install GNU core utilities via Homebrew to guarantee compatibility for complex agent workflows:
```bash
brew install coreutils findutils gnu-tar gnu-sed gawk gnutls gnu-indent gnu-getopt grep
```

## Permissions & Privacy Settings

If your Euxis agent requires access to sensitive user directories (`~/Documents`, `~/Downloads`, etc.), macOS Gatekeeper will silently block the Extism Sandbox or the Gateway daemon routing the filesystem operation.

1. Open **System Settings > Privacy & Security > Full Disk Access**.
2. Grant explicit full disk access to your terminal emulator (`Terminal.app`, `iTerm2`, or `Alacritty`).

## Architecture Pathing Rules

Euxis plugins and tools respect standard `$PATH` configurations. Keep in mind that Homebrew installs binaries differently based on your silicon architecture:

* **Apple Silicon (M1/M2/M3):** `/opt/homebrew/bin/`
* **Intel (x86_64):** `/usr/local/bin/`

Ensure your `.zshrc` explicitly prepends the correct directory so the Gateway can discover custom toolchains or linters like `ruff` or `cargo`.
