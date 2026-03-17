#pragma once

#include <memory>
#include <string>
#include <string_view>

namespace euxis::cli::tui {

/// Detected platform type.
enum class PlatformType { Linux, MacOS, WSL };

/// Abstract cross-platform adapter for OS-specific operations.
class Platform {
public:
    virtual ~Platform() = default;

    /// Get the detected platform type.
    [[nodiscard]] virtual PlatformType type() const = 0;

    /// Human-readable platform name.
    [[nodiscard]] virtual std::string name() const = 0;

    /// Copy text to system clipboard.
    [[nodiscard]] virtual bool clipboard_copy(std::string_view text) const = 0;

    /// Read text from system clipboard.
    [[nodiscard]] virtual std::string clipboard_paste() const = 0;

    /// Open a URL in the default browser.
    [[nodiscard]] virtual bool open_url(std::string_view url) const = 0;

    /// Returns true if SIGWINCH is reliable for resize detection.
    [[nodiscard]] virtual bool sigwinch_reliable() const = 0;

    /// Returns the recommended resize poll interval (ms), or 0 if polling is not needed.
    [[nodiscard]] virtual int resize_poll_interval_ms() const = 0;

    /// Returns true if the terminal supports truecolor.
    [[nodiscard]] virtual bool supports_truecolor() const = 0;

    /// Detect and create the appropriate platform adapter.
    [[nodiscard]] static std::unique_ptr<Platform> detect();

    /// Detect the platform type from environment.
    [[nodiscard]] static PlatformType detect_type();
};

/// Linux platform adapter.
class LinuxPlatform : public Platform {
public:
    [[nodiscard]] PlatformType type() const override { return PlatformType::Linux; }
    [[nodiscard]] std::string name() const override { return "Linux"; }
    [[nodiscard]] bool clipboard_copy(std::string_view text) const override;
    [[nodiscard]] std::string clipboard_paste() const override;
    [[nodiscard]] bool open_url(std::string_view url) const override;
    [[nodiscard]] bool sigwinch_reliable() const override { return true; }
    [[nodiscard]] int resize_poll_interval_ms() const override { return 0; }
    [[nodiscard]] bool supports_truecolor() const override;

private:
    [[nodiscard]] bool has_wayland() const;
};

/// macOS platform adapter.
class MacOSPlatform : public Platform {
public:
    [[nodiscard]] PlatformType type() const override { return PlatformType::MacOS; }
    [[nodiscard]] std::string name() const override { return "macOS"; }
    [[nodiscard]] bool clipboard_copy(std::string_view text) const override;
    [[nodiscard]] std::string clipboard_paste() const override;
    [[nodiscard]] bool open_url(std::string_view url) const override;
    [[nodiscard]] bool sigwinch_reliable() const override { return true; }
    [[nodiscard]] int resize_poll_interval_ms() const override { return 0; }
    [[nodiscard]] bool supports_truecolor() const override;
};

/// WSL (Windows Subsystem for Linux) platform adapter.
class WSLPlatform : public Platform {
public:
    [[nodiscard]] PlatformType type() const override { return PlatformType::WSL; }
    [[nodiscard]] std::string name() const override { return "WSL"; }
    [[nodiscard]] bool clipboard_copy(std::string_view text) const override;
    [[nodiscard]] std::string clipboard_paste() const override;
    [[nodiscard]] bool open_url(std::string_view url) const override;
    [[nodiscard]] bool sigwinch_reliable() const override { return is_wsl2_; }
    [[nodiscard]] int resize_poll_interval_ms() const override { return is_wsl2_ ? 0 : 500; }
    [[nodiscard]] bool supports_truecolor() const override;

    /// Detect whether running under WSL1 or WSL2.
    [[nodiscard]] bool is_wsl2() const { return is_wsl2_; }

    WSLPlatform();

private:
    bool is_wsl2_{false};
};

} // namespace euxis::cli::tui
