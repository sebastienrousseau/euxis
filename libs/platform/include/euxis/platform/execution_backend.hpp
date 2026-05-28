/// @file
/// @brief Pluggable command-execution backends (local fork+exec, Docker).
///
/// Mirrors the Hermes Agent tools/environments/{base,local,docker}.py
/// pattern: one abstract interface (IExecutionBackend) and concrete
/// implementations (LocalBackend, DockerBackend). The two backends here
/// are the ones that actually matter for compliance audits — local for
/// the dev loop, Docker for CI environments that mirror production.
/// The Hermes set of 7 backends (SSH, Singularity, Modal, Daytona,
/// Vercel Sandbox) is deliberately out of scope: those are personal-
/// assistant niches.
///
/// Orthogonal to libs/platform/sandbox.hpp (seccomp). Seccomp is a
/// post-fork hardening primitive used inside a local exec; this header
/// is about *which environment* the exec happens in.
#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace euxis::platform {

/// @brief Outcome of running a command through an IExecutionBackend.
///
/// `exit_code` is the actual program exit status when the backend
/// itself succeeded. When the backend failed (image missing, fork
/// failure, timeout, etc.) `error` is populated and `exit_code` is
/// set to a sentinel of -1.
struct ExecutionResult {
    int exit_code{-1};
    std::string stdout_text;
    std::string stderr_text;
    std::chrono::milliseconds wall_time{0};
    std::string backend_name;
    std::optional<std::string> error;
};

/// @brief Specification of one command to run.
///
/// Docker-only fields are silently ignored by LocalBackend.
struct ExecutionRequest {
    std::vector<std::string> argv;
    std::optional<std::filesystem::path> working_dir;
    std::vector<std::pair<std::string, std::string>> env;
    std::chrono::milliseconds timeout{std::chrono::milliseconds{60'000}};
    std::optional<std::string> stdin_text;

    /// @brief Docker-only: container image (default: "alpine:latest").
    std::string docker_image{"alpine:latest"};

    /// @brief Docker-only: read-only bind mounts {host_path, container_path}.
    std::vector<std::pair<std::filesystem::path, std::string>>
        docker_read_mounts;

    /// @brief Docker-only: read-write bind mounts.
    std::vector<std::pair<std::filesystem::path, std::string>>
        docker_write_mounts;
};

/// @brief Pluggable backend that executes a request and returns a result.
class IExecutionBackend {
public:
    virtual ~IExecutionBackend() = default;

    [[nodiscard]] virtual auto name() const noexcept -> std::string_view = 0;

    /// @brief Run the request. Never throws; failure is reported via
    ///        ExecutionResult::error.
    [[nodiscard]] virtual auto execute(const ExecutionRequest& req)
        -> ExecutionResult = 0;
};

/// @brief Fork + exec on the host. The default backend.
class LocalBackend final : public IExecutionBackend {
public:
    [[nodiscard]] auto name() const noexcept -> std::string_view override;

    [[nodiscard]] auto execute(const ExecutionRequest& req)
        -> ExecutionResult override;
};

/// @brief Spawn `docker run` per request.
///
/// Requires the docker CLI on PATH; does NOT speak the Docker socket
/// directly. This keeps the dependency surface zero (no libcurl, no
/// libdocker) at the cost of forking docker(1) per command — fine for
/// audit workloads which are coarse-grained.
class DockerBackend final : public IExecutionBackend {
public:
    [[nodiscard]] auto name() const noexcept -> std::string_view override;

    [[nodiscard]] auto execute(const ExecutionRequest& req)
        -> ExecutionResult override;

    /// @brief True when the docker CLI is on PATH and the daemon
    ///        responds to `docker info`. Cheap to call (forks once,
    ///        timeout 2s).
    [[nodiscard]] static auto is_available() noexcept -> bool;
};

} // namespace euxis::platform
