# 🐧 Running Euxis on Linux (The "Native" Guide)

Linux is the absolute native environment for the Euxis autonomous mesh. However, due to the fragmentation of the Linux ecosystem, you must configure certain generic daemon structures correctly.

## Display Servers: Wayland vs. X11

If you are orchestrating agents intended to interact with your system clipboard, or if you are running experimental Euxis GUI components, you must account for your display server:

* **Wayland:** Ensure you have `wl-clipboard` installed so agents can invoke `wl-copy` and `wl-paste`.
* **X11:** Ensure you have `xclip` or `xsel` installed.

## Keyring Management

Euxis relies on standard Keyring APIs to prevent storing plain-text cryptographic signatures and API Gateway tokens.

On Linux, ensure you have a valid `libsecret` compatible daemon running (like GNOME Keyring or KWallet). If you are running Euxis on a headless server without dbus/libsecret, Euxis will gracefully fall back to executing exclusively via your `~/.bashrc` exposed string variables natively.

## Init Systems (systemd)

For production deployments, you should run the Euxis Gateway as a background service via systemd. 

Create a user-level service at `~/.config/systemd/user/euxis-gateway.service`:

```ini
[Unit]
Description=Euxis Gateway Orchestrator
After=network.target

[Service]
Type=simple
ExecStart=%h/.local/bin/euxis gateway serve
Restart=always
RestartSec=3

[Install]
WantedBy=default.target
```

Enable and start the service locally:
```bash
systemctl --user enable euxis-gateway
systemctl --user start euxis-gateway
```
