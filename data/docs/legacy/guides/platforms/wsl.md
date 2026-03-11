# 🐧 Running Euxis on WSL (Windows Subsystem for Linux)

While Euxis runs natively on Linux, running it within WSL involves a "bridge" between two operating systems. To get the best experience, follow these specific configurations.

## 🚀 The Golden Rule: Stay in the Linux Filesystem

For maximum performance, **never run Euxis commands on files located in `/mnt/c/`** (your Windows `C:` drive).

Why? The protocol that translates Windows files to Linux (9P) is significantly slower than native Linux speeds.

**The Fix:** Always keep your Euxis configuration and data in your Linux home directory: `~/home/username/.euxis/`.

## 📋 Clipboard Integration

WSL does not share a clipboard with Windows by default. If you want Euxis to interact with your system clipboard, add this to your `.bashrc` or `.zshrc`:

```bash
# Redirect Linux clip to Windows clip.exe
alias xclip="clip.exe"
```

## 🌐 Networking & Localhost

If Euxis starts a local server (e.g., a web UI or API at `localhost:8080`), Windows usually forwards this automatically.

If you cannot reach Euxis from your Windows browser:
1. **Identify your WSL IP:** `hostname -I`
2. **Ensure Interface Binding:** Ensure Euxis is binding to `0.0.0.0` rather than `127.0.0.2`.
3. **Firewall:** Check your Windows Firewall to allow traffic on that specific port.

## 🔑 Environment Variables

To share your Windows API keys or environment variables with Euxis in WSL, use the `WSLENV` bridge.

In a Windows PowerShell:

```powershell
setx EUXIS_API_KEY "your_key_here"
setx WSLENV "EUXIS_API_KEY/u"
```
The `/u` flag ensures the variable is available when you open your WSL terminal.

## 🛠️ Troubleshooting WSL

**High Memory Usage:** WSL (v2) can sometimes "hog" RAM. If Euxis feels sluggish, create a `.wslconfig` file in your Windows `%USERPROFILE%` to limit memory.

**Systemd:** If Euxis requires systemd for background services, ensure your `/etc/wsl.conf` has:

```ini
[boot]
systemd=true
```
