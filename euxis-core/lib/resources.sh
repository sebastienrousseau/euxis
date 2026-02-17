#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Copyright (c) 2024-2026 Euxis Contributors
#
# resources.sh — System resource detection and throttling for agent dispatch
#
# Provides functions to:
#   - Detect available CPU, RAM, and current load
#   - Calculate optimal agent concurrency
#   - Monitor and throttle during execution
#   - Prevent system overload and overheating
#
# Usage:
#   source "${EUXIS_HOME}/euxis-core/lib/resources.sh"
#   max_agents=$(resource_max_concurrent_agents)
#   resource_wait_if_overloaded

# ============================================================================
# Configuration (can be overridden via environment)
# ============================================================================

# Maximum CPU usage % before throttling (default: 80%)
EUXIS_CPU_THRESHOLD="${EUXIS_CPU_THRESHOLD:-80}"

# Maximum memory usage % before throttling (default: 75%)
# Note: 85% is danger zone on macOS where swap hits SSD hard
# 75% leaves headroom for UI responsiveness and file caching
EUXIS_MEM_THRESHOLD="${EUXIS_MEM_THRESHOLD:-75}"

# Load threshold mode: "dynamic" (nproc * factor) or "static" (fixed value)
# Dynamic is recommended as it scales with core count
EUXIS_LOAD_MODE="${EUXIS_LOAD_MODE:-dynamic}"

# Dynamic load factor: threshold = nproc * factor (default: 0.8)
# On 8-core: threshold = 6.4, on 16-core: threshold = 12.8
EUXIS_LOAD_FACTOR="${EUXIS_LOAD_FACTOR:-0.8}"

# Static load threshold per core (only used if EUXIS_LOAD_MODE=static)
EUXIS_LOAD_THRESHOLD="${EUXIS_LOAD_THRESHOLD:-1.5}"

# Minimum agents to allow even under load (default: 1)
EUXIS_MIN_AGENTS="${EUXIS_MIN_AGENTS:-1}"

# Maximum agents regardless of resources (default: 8)
EUXIS_MAX_AGENTS="${EUXIS_MAX_AGENTS:-8}"

# Seconds to wait when throttled (default: 5)
EUXIS_THROTTLE_WAIT="${EUXIS_THROTTLE_WAIT:-5}"

# Stagger delay between agent launches to prevent thermal spikes (default: 2s)
EUXIS_STAGGER_DELAY="${EUXIS_STAGGER_DELAY:-2}"

# Enable verbose resource logging (default: false)
EUXIS_RESOURCE_VERBOSE="${EUXIS_RESOURCE_VERBOSE:-false}"

# ============================================================================
# Platform Detection
# ============================================================================

_resource_platform() {
    case "$(uname -s)" in
        Linux*)  echo "linux" ;;
        Darwin*) echo "macos" ;;
        MINGW*|CYGWIN*|MSYS*) echo "windows" ;;
        *)       echo "unknown" ;;
    esac
}

# ============================================================================
# CPU Detection
# ============================================================================

# Get number of CPU cores
resource_cpu_cores() {
    local platform
    platform=$(_resource_platform)

    case "$platform" in
        linux)
            nproc 2>/dev/null || grep -c ^processor /proc/cpuinfo 2>/dev/null || echo 4
            ;;
        macos)
            sysctl -n hw.ncpu 2>/dev/null || echo 4
            ;;
        *)
            echo 4  # Safe default
            ;;
    esac
}

# Get current CPU usage percentage (0-100)
# Note: On Linux, reading /proc/stat is nearly "free" in CPU cost
# On macOS, we use sysctl which is lightweight (avoid top which is heavy)
resource_cpu_usage() {
    local platform
    platform=$(_resource_platform)

    case "$platform" in
        linux)
            # Use /proc/stat - nearly zero overhead on Linux/WSL
            if [[ -f /proc/stat ]]; then
                local cpu_line1 cpu_line2
                cpu_line1=$(head -1 /proc/stat)
                sleep 0.1
                cpu_line2=$(head -1 /proc/stat)

                local idle1 total1 idle2 total2
                read -r _ user1 nice1 system1 idle1 iowait1 irq1 softirq1 _ <<< "$cpu_line1"
                read -r _ user2 nice2 system2 idle2 iowait2 irq2 softirq2 _ <<< "$cpu_line2"

                # Default to 0 if any value is empty
                : "${user1:=0}" "${nice1:=0}" "${system1:=0}" "${idle1:=0}" "${iowait1:=0}" "${irq1:=0}" "${softirq1:=0}"
                : "${user2:=0}" "${nice2:=0}" "${system2:=0}" "${idle2:=0}" "${iowait2:=0}" "${irq2:=0}" "${softirq2:=0}"

                total1=$((user1 + nice1 + system1 + idle1 + iowait1 + irq1 + softirq1))
                total2=$((user2 + nice2 + system2 + idle2 + iowait2 + irq2 + softirq2))

                local diff_idle=$((idle2 - idle1))
                local diff_total=$((total2 - total1))

                if [[ $diff_total -gt 0 ]]; then
                    echo $(( (diff_total - diff_idle) * 100 / diff_total ))
                else
                    echo 0
                fi
            else
                echo 50  # Fallback
            fi
            ;;
        macos)
            # Use sysctl - lightweight, preferred over top which is CPU-heavy
            # Get CPU load from vm.loadavg and convert to approximate usage %
            local load cores
            load=$(sysctl -n vm.loadavg 2>/dev/null | awk '{print $2}' | tr -d '{}')
            cores=$(sysctl -n hw.ncpu 2>/dev/null || echo 4)
            if [[ -n "$load" && -n "$cores" ]]; then
                # Convert load to approximate CPU %: (load / cores) * 100, capped at 100
                awk "BEGIN {pct = ($load / $cores) * 100; if (pct > 100) pct = 100; printf \"%d\", pct}"
            else
                echo 50  # Fallback
            fi
            ;;
        *)
            echo 50  # Safe default
            ;;
    esac
}

# Get CPU temperature (if available)
resource_cpu_temp() {
    local platform
    platform=$(_resource_platform)

    case "$platform" in
        linux)
            # Try various thermal zone files
            local temp_file
            for temp_file in /sys/class/thermal/thermal_zone*/temp; do
                if [[ -r "$temp_file" ]]; then
                    local temp
                    temp=$(cat "$temp_file" 2>/dev/null)
                    if [[ -n "$temp" && "$temp" -gt 0 ]]; then
                        echo $((temp / 1000))  # Convert millidegrees to degrees
                        return
                    fi
                fi
            done
            echo "unknown"
            ;;
        macos)
            # Requires additional tools on macOS
            echo "unknown"
            ;;
        *)
            echo "unknown"
            ;;
    esac
}

# ============================================================================
# Memory Detection
# ============================================================================

# Get total RAM in MB
resource_ram_total() {
    local platform
    platform=$(_resource_platform)

    case "$platform" in
        linux)
            awk '/MemTotal/ {print int($2/1024)}' /proc/meminfo 2>/dev/null || echo 8192
            ;;
        macos)
            echo $(($(sysctl -n hw.memsize 2>/dev/null || echo 8589934592) / 1024 / 1024))
            ;;
        *)
            echo 8192  # 8GB default
            ;;
    esac
}

# Get available RAM in MB
resource_ram_available() {
    local platform
    platform=$(_resource_platform)

    case "$platform" in
        linux)
            awk '/MemAvailable/ {print int($2/1024)}' /proc/meminfo 2>/dev/null || \
            awk '/MemFree/ {print int($2/1024)}' /proc/meminfo 2>/dev/null || echo 4096
            ;;
        macos)
            local pages_free page_size
            pages_free=$(vm_stat 2>/dev/null | awk '/Pages free/ {gsub(/\./,"",$3); print $3}')
            page_size=$(pagesize 2>/dev/null || echo 4096)
            echo $(( (pages_free * page_size) / 1024 / 1024 ))
            ;;
        *)
            echo 4096  # 4GB default
            ;;
    esac
}

# Get memory usage percentage
resource_mem_usage() {
    local total available
    total=$(resource_ram_total)
    available=$(resource_ram_available)

    if [[ $total -gt 0 ]]; then
        echo $(( (total - available) * 100 / total ))
    else
        echo 50
    fi
}

# ============================================================================
# Load Average
# ============================================================================

# Get 1-minute load average
resource_load_avg() {
    local platform
    platform=$(_resource_platform)

    case "$platform" in
        linux|macos)
            awk '{printf "%.2f", $1}' /proc/loadavg 2>/dev/null || \
            uptime | awk -F'load average:' '{print $2}' | awk -F',' '{printf "%.2f", $1}' || \
            echo "1.00"
            ;;
        *)
            echo "1.00"
            ;;
    esac
}

# Get load per core (load_avg / cores)
resource_load_per_core() {
    local load_avg cores
    load_avg=$(resource_load_avg)
    cores=$(resource_cpu_cores)

    awk "BEGIN {printf \"%.2f\", $load_avg / $cores}"
}

# ============================================================================
# Concurrency Calculation
# ============================================================================

# Calculate maximum concurrent agents based on system resources
resource_max_concurrent_agents() {
    local cores cpu_usage mem_usage load_per_core
    cores=$(resource_cpu_cores)
    cpu_usage=$(resource_cpu_usage)
    mem_usage=$(resource_mem_usage)
    load_per_core=$(resource_load_per_core)

    local max_agents=$EUXIS_MAX_AGENTS

    # Start with cores / 2 as baseline (leave room for system)
    local baseline=$((cores / 2))
    [[ $baseline -lt 1 ]] && baseline=1
    [[ $baseline -gt $max_agents ]] && baseline=$max_agents

    # Reduce if CPU is high
    if [[ $cpu_usage -gt $EUXIS_CPU_THRESHOLD ]]; then
        baseline=$((baseline / 2))
        [[ "$EUXIS_RESOURCE_VERBOSE" == "true" ]] && \
            echo "[resource] CPU at ${cpu_usage}% (>${EUXIS_CPU_THRESHOLD}%), reducing to $baseline agents" >&2
    fi

    # Reduce if memory is high
    if [[ $mem_usage -gt $EUXIS_MEM_THRESHOLD ]]; then
        baseline=$((baseline / 2))
        [[ "$EUXIS_RESOURCE_VERBOSE" == "true" ]] && \
            echo "[resource] Memory at ${mem_usage}% (>${EUXIS_MEM_THRESHOLD}%), reducing to $baseline agents" >&2
    fi

    # Reduce if load is high
    local load_threshold_x100=$((${EUXIS_LOAD_THRESHOLD%.*} * 100))
    local load_x100
    load_x100=$(awk "BEGIN {printf \"%d\", $load_per_core * 100}")

    if [[ $load_x100 -gt $load_threshold_x100 ]]; then
        baseline=$((baseline / 2))
        [[ "$EUXIS_RESOURCE_VERBOSE" == "true" ]] && \
            echo "[resource] Load at ${load_per_core}/core (>${EUXIS_LOAD_THRESHOLD}), reducing to $baseline agents" >&2
    fi

    # Enforce minimum
    [[ $baseline -lt $EUXIS_MIN_AGENTS ]] && baseline=$EUXIS_MIN_AGENTS

    echo "$baseline"
}

# ============================================================================
# Throttling
# ============================================================================

# Calculate effective load threshold based on mode
_resource_load_threshold() {
    local cores
    cores=$(resource_cpu_cores)

    if [[ "$EUXIS_LOAD_MODE" == "dynamic" ]]; then
        # Dynamic: threshold = nproc * factor
        # On 8-core with 0.8 factor: threshold = 6.4
        # On 16-core with 0.8 factor: threshold = 12.8
        awk "BEGIN {printf \"%.2f\", $cores * $EUXIS_LOAD_FACTOR}"
    else
        # Static: threshold = cores * per-core-threshold
        awk "BEGIN {printf \"%.2f\", $cores * $EUXIS_LOAD_THRESHOLD}"
    fi
}

# Check if system is overloaded
resource_is_overloaded() {
    local cpu_usage mem_usage load_avg load_threshold
    cpu_usage=$(resource_cpu_usage)
    mem_usage=$(resource_mem_usage)
    load_avg=$(resource_load_avg)
    load_threshold=$(_resource_load_threshold)

    # Convert to integers for comparison (x100)
    local load_x100 threshold_x100
    load_x100=$(awk "BEGIN {printf \"%d\", $load_avg * 100}")
    threshold_x100=$(awk "BEGIN {printf \"%d\", $load_threshold * 100}")

    if [[ $cpu_usage -gt $EUXIS_CPU_THRESHOLD ]] || \
       [[ $mem_usage -gt $EUXIS_MEM_THRESHOLD ]] || \
       [[ $load_x100 -gt $threshold_x100 ]]; then
        return 0  # true, is overloaded
    fi
    return 1  # false, not overloaded
}

# Wait if system is overloaded (with timeout)
resource_wait_if_overloaded() {
    local max_wait="${1:-60}"  # Default max wait: 60 seconds
    local waited=0

    while resource_is_overloaded && [[ $waited -lt $max_wait ]]; do
        [[ "$EUXIS_RESOURCE_VERBOSE" == "true" ]] && \
            echo "[resource] System overloaded, waiting ${EUXIS_THROTTLE_WAIT}s... (${waited}s/${max_wait}s)" >&2
        sleep "$EUXIS_THROTTLE_WAIT"
        waited=$((waited + EUXIS_THROTTLE_WAIT))
    done

    if [[ $waited -ge $max_wait ]]; then
        return 1  # Timed out waiting
    fi
    return 0
}

# ============================================================================
# Status / Reporting
# ============================================================================

# Print current resource status
resource_status() {
    local cores cpu_usage mem_usage mem_total mem_avail load_avg load_per_core temp max_agents load_threshold platform

    platform=$(_resource_platform)
    cores=$(resource_cpu_cores)
    cpu_usage=$(resource_cpu_usage)
    mem_total=$(resource_ram_total)
    mem_avail=$(resource_ram_available)
    mem_usage=$(resource_mem_usage)
    load_avg=$(resource_load_avg)
    load_per_core=$(resource_load_per_core)
    load_threshold=$(_resource_load_threshold)
    temp=$(resource_cpu_temp)
    max_agents=$(resource_max_concurrent_agents)

    echo "┌─────────────────────────────────────────────"
    echo "│ System Resources ($platform)"
    echo "├─────────────────────────────────────────────"
    printf "│ CPU:     %d cores, %d%% used" "$cores" "$cpu_usage"
    [[ "$temp" != "unknown" ]] && printf ", %d°C" "$temp"
    echo ""
    printf "│ Memory:  %dMB / %dMB (%d%% used, threshold %d%%)\n" "$((mem_total - mem_avail))" "$mem_total" "$mem_usage" "$EUXIS_MEM_THRESHOLD"
    printf "│ Load:    %s / %.1f threshold (%.2f per core)\n" "$load_avg" "$load_threshold" "$load_per_core"
    echo "├─────────────────────────────────────────────"
    printf "│ Max concurrent agents: %d\n" "$max_agents"
    printf "│ Stagger delay: %ds between launches\n" "$EUXIS_STAGGER_DELAY"
    local nice_val="${EUXIS_NICE:-15}"
    if [[ "$nice_val" == "0" ]]; then
        echo "│ Process priority: normal (nice disabled)"
    else
        printf "│ Process priority: nice -n %s (background)\n" "$nice_val"
    fi

    if resource_is_overloaded; then
        echo "│ Status:  ⚠️  THROTTLED"
    else
        echo "│ Status:  ✓ Ready"
    fi
    echo "└─────────────────────────────────────────────"
}

# Export recommended settings as JSON
resource_config_json() {
    local cores cpu_usage mem_usage load_per_core max_agents overloaded

    cores=$(resource_cpu_cores)
    cpu_usage=$(resource_cpu_usage)
    mem_usage=$(resource_mem_usage)
    load_per_core=$(resource_load_per_core)
    max_agents=$(resource_max_concurrent_agents)

    if resource_is_overloaded; then
        overloaded="true"
    else
        overloaded="false"
    fi

    cat <<EOF
{
  "cpu_cores": $cores,
  "cpu_usage_percent": $cpu_usage,
  "mem_usage_percent": $mem_usage,
  "load_per_core": $load_per_core,
  "max_concurrent_agents": $max_agents,
  "is_overloaded": $overloaded,
  "thresholds": {
    "cpu": $EUXIS_CPU_THRESHOLD,
    "memory": $EUXIS_MEM_THRESHOLD,
    "load": $EUXIS_LOAD_THRESHOLD
  }
}
EOF
}
