#!/usr/bin/env bash
# Euxis Library: Provider routing and execution
[[ -n "${_EUXIS_LIB_PROVIDERS:-}" ]] && return; _EUXIS_LIB_PROVIDERS=1

set -euo pipefail


EUXIS_HOME="${EUXIS_HOME:-${HOME}/.euxis}"
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "${SCRIPT_DIR}/../.." && pwd)"

COMMON_SH="${EUXIS_HOME}/euxis-core/lib/common.sh"
if [[ -f "${COMMON_SH}" ]]; then
  # shellcheck source=core/lib/common.sh
  source "${COMMON_SH}"
else
  # shellcheck source=core/lib/common.sh
  source "${REPO_ROOT}/euxis-core/lib/common.sh"
fi

# Default provider (fallback when no tiering match)
DEFAULT_PROVIDER="${EUXIS_DEFAULT_PROVIDER:-claude}"

# Source router for cost-optimized model selection
ROUTER_SH="${EUXIS_HOME}/euxis-core/lib/router.sh"
if [[ -f "$ROUTER_SH" ]]; then
    # shellcheck source=euxis-core/lib/router.sh
    source "$ROUTER_SH"
    ROUTER_INTEGRATED=true
else
    ROUTER_INTEGRATED=false
fi

# Configurable timeouts for provider API calls (in seconds)
EUXIS_API_TIMEOUT="${EUXIS_API_TIMEOUT:-300}"  # 5 minutes default
# Resolve active provider from current CLI session context.
# Priority:
# 1) Explicit EUXIS_SESSION_PROVIDER override
# 2) Claude Code session env
# 3) Codex session env
resolve_session_provider() {
    if [[ -n "${EUXIS_SESSION_PROVIDER:-}" ]]; then
        echo "${EUXIS_SESSION_PROVIDER}"
        return 0
    fi

    if [[ -n "${CLAUDECODE:-}" || -n "${CLAUDE_CODE_ENTRYPOINT:-}" ]]; then
        echo "claude"
        return 0
    fi

    if [[ -n "${CODEX_THREAD_ID:-}" || -n "${CODEX_CI:-}" || -n "${CODEX_SESSION_ID:-}" ]]; then
        echo "openai"
        return 0
    fi

    return 1
}

# ============================================================================
# Provider Fallback Chain (v0.0.2)
# Automatic failover when primary provider is unavailable.
# Ensures mission-critical tasks (guard, verify) are never completely offline.
# ============================================================================

# Fallback chain: cloud -> cloud alternative -> local
PROVIDER_FALLBACK_CHAIN="${EUXIS_FALLBACK_CHAIN:-claude:gemini:openai:kiro-cli:qwen:crush:goose:ollama}"

# Max retries before switching providers
EUXIS_MAX_RETRIES="${EUXIS_MAX_RETRIES:-2}"

# Retry backoff (seconds)
EUXIS_RETRY_BACKOFF="${EUXIS_RETRY_BACKOFF:-2}"
EUXIS_MAX_BACKOFF="${EUXIS_MAX_BACKOFF:-60}"

# Quota governor thresholds (percent of configured cap)
EUXIS_THROTTLE_THRESHOLD_PCT="${EUXIS_THROTTLE_THRESHOLD_PCT:-40}"
EUXIS_FAILOVER_THRESHOLD_PCT="${EUXIS_FAILOVER_THRESHOLD_PCT:-50}"
EUXIS_THROTTLE_STEP_SEC="${EUXIS_THROTTLE_STEP_SEC:-1}"

# Usage tracking state
EUXIS_PROVIDER_USAGE_DIR="${EUXIS_PROVIDER_USAGE_DIR:-${EUXIS_HOME}/euxis-runtime/data/provider-usage}"
EUXIS_DEFERRED_QUEUE="${EUXIS_DEFERRED_QUEUE:-${EUXIS_PROVIDER_USAGE_DIR}/deferred-tasks.jsonl}"
EUXIS_PROVIDER_INITIAL="${EUXIS_PROVIDER_INITIAL:-}"
EUXIS_PROVIDER_CURRENT="${EUXIS_PROVIDER_CURRENT:-}"
EUXIS_PROVIDER_TRANSITIONS="${EUXIS_PROVIDER_TRANSITIONS:-[]}"
EUXIS_SEMANTIC_CACHE_ENABLE="${EUXIS_SEMANTIC_CACHE_ENABLE:-1}"
EUXIS_SEMANTIC_CACHE_TTL_SECONDS="${EUXIS_SEMANTIC_CACHE_TTL_SECONDS:-86400}"
EUXIS_SEMANTIC_CACHE_DIR="${EUXIS_SEMANTIC_CACHE_DIR:-${EUXIS_PROVIDER_USAGE_DIR}/semantic-cache}"
EUXIS_VECTOR_CACHE_ENABLE="${EUXIS_VECTOR_CACHE_ENABLE:-1}"
EUXIS_VECTOR_CACHE_BACKEND="${EUXIS_VECTOR_CACHE_BACKEND:-valkey}" # valkey|file
EUXIS_VECTOR_CACHE_DIM="${EUXIS_VECTOR_CACHE_DIM:-256}"
EUXIS_VECTOR_THRESHOLD_DEFAULT="${EUXIS_VECTOR_THRESHOLD_DEFAULT:-0.92}"
EUXIS_VECTOR_THRESHOLD_TECHNICAL="${EUXIS_VECTOR_THRESHOLD_TECHNICAL:-0.95}"
EUXIS_PLAN_CACHE_ENABLE="${EUXIS_PLAN_CACHE_ENABLE:-1}"
EUXIS_PLAN_CACHE_THRESHOLD="${EUXIS_PLAN_CACHE_THRESHOLD:-0.96}"
EUXIS_VALKEY_URL="${EUXIS_VALKEY_URL:-redis://127.0.0.2:6379}"
EUXIS_CONTEXT_COMPACTION_ENABLE="${EUXIS_CONTEXT_COMPACTION_ENABLE:-1}"
EUXIS_CONTEXT_COMPACTION_TRIGGER_PCT="${EUXIS_CONTEXT_COMPACTION_TRIGGER_PCT:-70}"
EUXIS_CONTEXT_COMPACTION_COOLDOWN_SECONDS="${EUXIS_CONTEXT_COMPACTION_COOLDOWN_SECONDS:-300}"
EUXIS_CONTEXT_COMPACTION_CMD="${EUXIS_CONTEXT_COMPACTION_CMD:-${EUXIS_HOME}/euxis-cli/bin/euxis-context-worker}"
EUXIS_OFFPEAK_ONLY_LOW_PRIORITY="${EUXIS_OFFPEAK_ONLY_LOW_PRIORITY:-0}"
EUXIS_OFFPEAK_START_HOUR="${EUXIS_OFFPEAK_START_HOUR:-22}"
EUXIS_OFFPEAK_END_HOUR="${EUXIS_OFFPEAK_END_HOUR:-6}"

_ensure_usage_dir() {
    mkdir -p "${EUXIS_PROVIDER_USAGE_DIR}" 2>/dev/null || true
}

_stat_mtime() {
    local f="$1"
    stat -c '%Y' "${f}" 2>/dev/null || stat -f '%m' "${f}" 2>/dev/null || echo 0
}

_cache_key() {
    local txt="$1"
    if command -v shasum &>/dev/null; then
        printf '%s' "${txt}" | shasum -a 256 | awk '{print $1}'
    else
        printf '%s' "${txt}" | md5 | awk '{print $NF}'
    fi
}

_normalize_for_semantic_cache() {
    local txt="${1,,}"
    txt=$(printf '%s' "${txt}" | sed -E 's/[0-9]+/#/g; s#[[:space:]]+# #g; s#[^a-z0-9_./:# -]##g')
    printf '%s' "${txt}"
}

_vector_threshold_for_task() {
    local task="${1,,}"
    if [[ "${task}" =~ code|refactor|compile|test|schema|migration|performance|latency|bug|stacktrace ]]; then
        printf '%s' "${EUXIS_VECTOR_THRESHOLD_TECHNICAL}"
    else
        printf '%s' "${EUXIS_VECTOR_THRESHOLD_DEFAULT}"
    fi
}

_vector_embed_text() {
    local txt="$1"
    local dim="${EUXIS_VECTOR_CACHE_DIM}"
    if command -v python3 &>/dev/null; then
        python3 - "${txt}" "${dim}" <<'PY'
import hashlib, json, re, sys
text = sys.argv[1].lower()
dim = int(sys.argv[2])
vec = [0.0] * dim
for tok in re.findall(r"[a-z0-9_./:#-]+", text):
    h = int(hashlib.sha1(tok.encode("utf-8")).hexdigest(), 16) % dim
    vec[h] += 1.0
norm = sum(v*v for v in vec) ** 0.5
if norm > 0:
    vec = [v / norm for v in vec]
print(json.dumps(vec, separators=(",", ":")))
PY
    else
        printf '[]'
    fi
}

_vector_cosine() {
    local a="$1"
    local b="$2"
    if command -v python3 &>/dev/null; then
        python3 - "${a}" "${b}" <<'PY'
import json, sys
a = json.loads(sys.argv[1])
b = json.loads(sys.argv[2])
if not a or not b or len(a) != len(b):
    print("0")
    raise SystemExit(0)
dot = sum(x*y for x,y in zip(a,b))
print(f"{dot:.6f}")
PY
    else
        printf '0'
    fi
}

_valkey_available() {
    command -v redis-cli &>/dev/null && redis-cli -u "${EUXIS_VALKEY_URL}" PING >/dev/null 2>&1
}

_vector_cache_store_valkey() {
    local ns="$1"
    local id="$2"
    local task="$3"
    local embedding="$4"
    local payload="$5"
    redis-cli -u "${EUXIS_VALKEY_URL}" HSET "euxis:v:${ns}:${id}" \
        task "${task}" \
        embedding "${embedding}" \
        payload "${payload}" \
        ts "$(date +%s)" >/dev/null 2>&1 || true
    redis-cli -u "${EUXIS_VALKEY_URL}" SADD "euxis:v:${ns}:ids" "${id}" >/dev/null 2>&1 || true
}

_vector_cache_lookup_valkey() {
    local ns="$1"
    local query_embedding="$2"
    local threshold="$3"
    local ids
    ids=$(redis-cli -u "${EUXIS_VALKEY_URL}" SMEMBERS "euxis:v:${ns}:ids" 2>/dev/null || true)
    [[ -n "${ids}" ]] || return 1

    local best_sim="0"
    local best_payload=""
    local id emb payload sim
    while IFS= read -r id; do
        [[ -n "${id}" ]] || continue
        emb=$(redis-cli -u "${EUXIS_VALKEY_URL}" HGET "euxis:v:${ns}:${id}" embedding 2>/dev/null || true)
        payload=$(redis-cli -u "${EUXIS_VALKEY_URL}" HGET "euxis:v:${ns}:${id}" payload 2>/dev/null || true)
        [[ -n "${emb}" && -n "${payload}" ]] || continue
        sim=$(_vector_cosine "${query_embedding}" "${emb}")
        if awk "BEGIN {exit !(${sim} > ${best_sim})}"; then
            best_sim="${sim}"
            best_payload="${payload}"
        fi
    done <<< "${ids}"

    if [[ -n "${best_payload}" ]] && awk "BEGIN {exit !(${best_sim} >= ${threshold})}"; then
        printf '%s' "${best_payload}"
        return 0
    fi
    return 1
}

_vector_cache_store_file() {
    local ns="$1"
    local id="$2"
    local task="$3"
    local embedding="$4"
    local payload="$5"
    local dir="${EUXIS_PROVIDER_USAGE_DIR}/vector-cache/${ns}"
    mkdir -p "${dir}" 2>/dev/null || true
    jq -n \
      --arg id "${id}" \
      --arg task "${task}" \
      --argjson emb "${embedding}" \
      --arg payload "${payload}" \
      --argjson ts "$(date +%s)" \
      '{id:$id,task:$task,embedding:$emb,payload:$payload,ts:$ts}' > "${dir}/${id}.json" 2>/dev/null || true
}

_vector_cache_lookup_file() {
    local ns="$1"
    local query_embedding="$2"
    local threshold="$3"
    local dir="${EUXIS_PROVIDER_USAGE_DIR}/vector-cache/${ns}"
    [[ -d "${dir}" ]] || return 1
    local best_sim="0"
    local best_payload=""
    local f emb payload sim
    for f in "${dir}"/*.json; do
        [[ -f "${f}" ]] || continue
        emb=$(jq -c '.embedding' "${f}" 2>/dev/null || true)
        payload=$(jq -r '.payload' "${f}" 2>/dev/null || true)
        [[ -n "${emb}" && "${emb}" != "null" && -n "${payload}" && "${payload}" != "null" ]] || continue
        sim=$(_vector_cosine "${query_embedding}" "${emb}")
        if awk "BEGIN {exit !(${sim} > ${best_sim})}"; then
            best_sim="${sim}"
            best_payload="${payload}"
        fi
    done
    if [[ -n "${best_payload}" ]] && awk "BEGIN {exit !(${best_sim} >= ${threshold})}"; then
        printf '%s' "${best_payload}"
        return 0
    fi
    return 1
}

_semantic_vector_lookup() {
    local ns="$1"
    local task="$2"
    local threshold="$3"
    [[ "${EUXIS_VECTOR_CACHE_ENABLE}" == "1" ]] || return 1
    local emb
    emb=$(_vector_embed_text "${task}")
    [[ -n "${emb}" && "${emb}" != "[]" ]] || return 1
    if [[ "${EUXIS_VECTOR_CACHE_BACKEND}" == "valkey" ]] && _valkey_available; then
        _vector_cache_lookup_valkey "${ns}" "${emb}" "${threshold}"
    else
        _vector_cache_lookup_file "${ns}" "${emb}" "${threshold}"
    fi
}

_semantic_vector_store() {
    local ns="$1"
    local task="$2"
    local payload="$3"
    [[ "${EUXIS_VECTOR_CACHE_ENABLE}" == "1" ]] || return 0
    local emb id
    emb=$(_vector_embed_text "${task}")
    [[ -n "${emb}" && "${emb}" != "[]" ]] || return 0
    id=$(_cache_key "${ns}|${task}|${payload}")
    if [[ "${EUXIS_VECTOR_CACHE_BACKEND}" == "valkey" ]] && _valkey_available; then
        _vector_cache_store_valkey "${ns}" "${id}" "${task}" "${emb}" "${payload}"
    else
        _vector_cache_store_file "${ns}" "${id}" "${task}" "${emb}" "${payload}"
    fi
}

_extract_plan_template() {
    local output="$1"
    if command -v python3 &>/dev/null; then
        python3 - "${output}" <<'PY'
import re, sys
text = sys.argv[1]
block = None
for pat in [r'(?is)##\s*(execution plan|plan|architecture plan)\s*\n(.*?)(\n##|\Z)',
            r'(?is)\b(plan)\s*:\s*\n(.*?)(\n\n|\Z)']:
    m = re.search(pat, text)
    if m:
        block = m.group(2).strip()
        break
if not block:
    lines = [ln for ln in text.splitlines() if re.match(r'^\s*[-*]\s+', ln)]
    block = "\n".join(lines[:20]).strip()
print(block, end="")
PY
    else
        printf '%s' "${output}" | awk '/^##[[:space:]]*(Execution Plan|Plan|Architecture Plan)/{flag=1;next}/^##/{if(flag)exit}flag' | head -n 80
    fi
}

_plan_cache_lookup() {
    local agent="$1"
    local task="$2"
    [[ "${EUXIS_PLAN_CACHE_ENABLE}" == "1" ]] || return 1
    _semantic_vector_lookup "plan:${agent}" "${task}" "${EUXIS_PLAN_CACHE_THRESHOLD}"
}

_plan_cache_store() {
    local agent="$1"
    local task="$2"
    local output="$3"
    [[ "${EUXIS_PLAN_CACHE_ENABLE}" == "1" ]] || return 0
    [[ "${agent}" == "architect" || "${agent}" == "planner" ]] || return 0
    local tpl
    tpl=$(_extract_plan_template "${output}")
    [[ -n "${tpl}" ]] || return 0
    _semantic_vector_store "plan:${agent}" "${task}" "${tpl}"
}

_semantic_cache_lookup() {
    local provider="$1"
    local agent="$2"
    local prompt="$3"
    local task="$4"
    [[ "${EUXIS_SEMANTIC_CACHE_ENABLE}" == "1" ]] || return 1
    _ensure_usage_dir
    mkdir -p "${EUXIS_SEMANTIC_CACHE_DIR}/${provider}/exact" "${EUXIS_SEMANTIC_CACHE_DIR}/${provider}/semantic" 2>/dev/null || true

    local exact_key exact_file
    exact_key=$(_cache_key "${provider}|${agent}|${prompt}")
    exact_file="${EUXIS_SEMANTIC_CACHE_DIR}/${provider}/exact/${exact_key}.txt"
    local now mtime age
    now=$(date +%s)
    if [[ -f "${exact_file}" ]]; then
        mtime=$(_stat_mtime "${exact_file}")
        age=$(( now - mtime ))
        if (( age <= EUXIS_SEMANTIC_CACHE_TTL_SECONDS )); then
            cat "${exact_file}"
            return 0
        fi
    fi

    local sem_key sem_file
    sem_key=$(_cache_key "$(_normalize_for_semantic_cache "${task}")")
    sem_file="${EUXIS_SEMANTIC_CACHE_DIR}/${provider}/semantic/${sem_key}.txt"
    if [[ -f "${sem_file}" ]]; then
        mtime=$(_stat_mtime "${sem_file}")
        age=$(( now - mtime ))
        if (( age <= EUXIS_SEMANTIC_CACHE_TTL_SECONDS )); then
            cat "${sem_file}"
            return 0
        fi
    fi
    return 1
}

_semantic_cache_store() {
    local provider="$1"
    local agent="$2"
    local prompt="$3"
    local task="$4"
    local output="$5"
    [[ "${EUXIS_SEMANTIC_CACHE_ENABLE}" == "1" ]] || return 0
    _ensure_usage_dir
    mkdir -p "${EUXIS_SEMANTIC_CACHE_DIR}/${provider}/exact" "${EUXIS_SEMANTIC_CACHE_DIR}/${provider}/semantic" 2>/dev/null || true
    local exact_key sem_key
    exact_key=$(_cache_key "${provider}|${agent}|${prompt}")
    sem_key=$(_cache_key "$(_normalize_for_semantic_cache "${task}")")
    printf '%s' "${output}" > "${EUXIS_SEMANTIC_CACHE_DIR}/${provider}/exact/${exact_key}.txt"
    printf '%s' "${output}" > "${EUXIS_SEMANTIC_CACHE_DIR}/${provider}/semantic/${sem_key}.txt"
}

_is_offpeak_now() {
    local start="${EUXIS_OFFPEAK_START_HOUR}"
    local end="${EUXIS_OFFPEAK_END_HOUR}"
    local hour
    hour=$(date +%H)
    hour=$((10#${hour}))
    if (( start < end )); then
        (( hour >= start && hour < end ))
    else
        (( hour >= start || hour < end ))
    fi
}

_provider_state_file() {
    local sid="${EUXIS_SESSION_ID:-${SESSION_ID:-default}}"
    echo "${EUXIS_PROVIDER_USAGE_DIR}/${sid}.provider-state.json"
}

_persist_provider_state() {
    _ensure_usage_dir
    local file
    file=$(_provider_state_file)
    jq -n \
        --arg initial "${EUXIS_PROVIDER_INITIAL:-}" \
        --arg current "${EUXIS_PROVIDER_CURRENT:-}" \
        --argjson transitions "${EUXIS_PROVIDER_TRANSITIONS:-[]}" \
        --arg ts "$(date -u +%Y-%m-%dT%H:%M:%SZ)" \
        '{
            initial_provider: $initial,
            current_provider: $current,
            transitions: $transitions,
            updated_at: $ts
        }' > "${file}" 2>/dev/null || true
}

_record_provider_transition() {
    local from="$1"
    local to="$2"
    local reason="$3"
    local entry
    entry=$(jq -nc \
        --arg from "${from}" \
        --arg to "${to}" \
        --arg reason "${reason}" \
        --arg ts "$(euxis_ts_utc_ms)" \
        '{from:$from,to:$to,reason:$reason,timestamp:$ts}')
    EUXIS_PROVIDER_TRANSITIONS=$(jq -nc \
        --argjson arr "${EUXIS_PROVIDER_TRANSITIONS:-[]}" \
        --argjson e "${entry}" \
        '$arr + [$e]') || EUXIS_PROVIDER_TRANSITIONS='[]'
    EUXIS_PROVIDER_CURRENT="${to}"
    _persist_provider_state

    # Optional time-travel event if flight recorder is active in current shell.
    if declare -F flight_snapshot >/dev/null 2>&1; then
        flight_snapshot "provider_transition" "${entry}" 2>/dev/null || true
    fi
}

get_provider_state_json() {
    local file
    file=$(_provider_state_file)
    if [[ -f "${file}" ]]; then
        cat "${file}"
        return 0
    fi

    jq -n \
        --arg initial "${EUXIS_PROVIDER_INITIAL:-}" \
        --arg current "${EUXIS_PROVIDER_CURRENT:-}" \
        --argjson transitions "${EUXIS_PROVIDER_TRANSITIONS:-[]}" \
        '{
            initial_provider: $initial,
            current_provider: $current,
            transitions: $transitions
        }'
}

_provider_cap_tokens() {
    local provider="$1"
    local up="${provider^^}"
    up="${up//-/_}"
    local key="EUXIS_PROVIDER_CAP_${up}"
    local cap="${!key:-${EUXIS_PROVIDER_TOKEN_CAP:-0}}"
    echo "${cap}"
}

_provider_window_seconds() {
    local provider="$1"
    local up="${provider^^}"
    up="${up//-/_}"
    local key="EUXIS_PROVIDER_WINDOW_${up}_SECONDS"
    local win="${!key:-${EUXIS_PROVIDER_WINDOW_SECONDS:-86400}}"
    echo "${win}"
}

_provider_usage_file() {
    local provider="$1"
    echo "${EUXIS_PROVIDER_USAGE_DIR}/${provider}.state"
}

_read_provider_usage() {
    local provider="$1"
    local file
    file=$(_provider_usage_file "${provider}")
    local now
    now=$(date +%s)
    local window_start="$now"
    local tokens_used=0
    local requests=0

    if [[ -f "${file}" ]]; then
        IFS='|' read -r window_start tokens_used requests < "${file}" || true
    fi

    window_start="${window_start:-$now}"
    tokens_used="${tokens_used:-0}"
    requests="${requests:-0}"

    local win
    win=$(_provider_window_seconds "${provider}")
    if (( now - window_start >= win )); then
        window_start="$now"
        tokens_used=0
        requests=0
    fi

    echo "${window_start}|${tokens_used}|${requests}"
}

_write_provider_usage() {
    local provider="$1"
    local window_start="$2"
    local tokens_used="$3"
    local requests="$4"
    local file
    file=$(_provider_usage_file "${provider}")
    printf '%s|%s|%s\n' "${window_start}" "${tokens_used}" "${requests}" > "${file}"
}

_estimate_tokens() {
    local txt="$1"
    if [[ -z "${txt}" ]]; then
        echo 0
        return
    fi
    echo $(( (${#txt} / 4) + 1 ))
}

_resolve_task_priority() {
    local task="${1:-}"
    if [[ -n "${EUXIS_TASK_PRIORITY:-}" ]]; then
        echo "${EUXIS_TASK_PRIORITY^^}"
        return
    fi
    case "${task}" in
        *"[P0]"*|*"[SEV0]"*) echo "P0" ;;
        *"[P1]"*|*"[SEV1]"*) echo "P1" ;;
        *"[P3]"*|*"[SEV3]"*) echo "P3" ;;
        *"[P4]"*|*"[SEV4]"*) echo "P4" ;;
        *) echo "P2" ;;
    esac
}

_is_low_priority() {
    local priority="$1"
    case "${priority}" in
        P3|P4|LOW) return 0 ;;
        *) return 1 ;;
    esac
}

_projected_utilization_pct() {
    local provider="$1"
    local projected_add="$2"
    local ratelimit_util
    ratelimit_util=$(_latest_ratelimit_utilization_pct "${provider}" 2>/dev/null || true)
    local cap
    cap=$(_provider_cap_tokens "${provider}")
    local util=0

    if [[ -n "${cap}" && "${cap}" -gt 0 ]]; then
        local state window_start used req
        state=$(_read_provider_usage "${provider}")
        IFS='|' read -r window_start used req <<< "${state}"
        util=$(( ((used + projected_add) * 100) / cap ))
    fi

    if [[ -n "${ratelimit_util}" ]] && [[ "${ratelimit_util}" =~ ^[0-9]+$ ]] && (( ratelimit_util > util )); then
        util="${ratelimit_util}"
    fi

    echo "${util}"
}

_context_compaction_state_file() {
    echo "${EUXIS_PROVIDER_USAGE_DIR}/context-compaction.state"
}

_context_compaction_ready() {
    local provider="$1"
    local util now file line last_provider last_ts
    util=$(_projected_utilization_pct "${provider}" 0)
    [[ "${util}" =~ ^[0-9]+$ ]] || return 1
    (( util >= EUXIS_CONTEXT_COMPACTION_TRIGGER_PCT )) || return 1

    file=$(_context_compaction_state_file)
    now=$(date +%s)
    if [[ -f "${file}" ]]; then
        line=$(cat "${file}" 2>/dev/null || true)
        IFS='|' read -r last_provider last_ts <<< "${line}"
        last_ts="${last_ts:-0}"
        if [[ "${last_provider}" == "${provider}" ]] && (( now - last_ts < EUXIS_CONTEXT_COMPACTION_COOLDOWN_SECONDS )); then
            return 1
        fi
    fi
    return 0
}

_maybe_trigger_context_compaction() {
    local provider="$1"
    local agent="$2"
    local task="$3"
    [[ "${EUXIS_CONTEXT_COMPACTION_ENABLE}" == "1" ]] || return 0
    _ensure_usage_dir
    _context_compaction_ready "${provider}" || return 0

    local now util cmd
    now=$(date +%s)
    util=$(_projected_utilization_pct "${provider}" 0)
    cmd="${EUXIS_CONTEXT_COMPACTION_CMD}"

    if [[ -x "${cmd}" ]]; then
        "${cmd}" once >/dev/null 2>&1 || true
    fi
    printf '%s|%s\n' "${provider}" "${now}" > "$(_context_compaction_state_file)"
    printf '{"ts":"%s","provider":"%s","agent":"%s","utilization_pct":%s,"task":"%s","status":"triggered"}\n' \
        "$(date -u +%Y-%m-%dT%H:%M:%SZ)" \
        "${provider}" \
        "${agent}" \
        "${util}" \
        "$(printf '%s' "${task}" | tr '\n' ' ' | sed 's/"/\\"/g')" \
        >> "${EUXIS_PROVIDER_USAGE_DIR}/context-compaction-events.jsonl"
}

_enqueue_deferred_task() {
    local provider="$1"
    local agent="$2"
    local priority="$3"
    local utilization="$4"
    local task="$5"
    _ensure_usage_dir
    printf '{"ts":"%s","provider":"%s","agent":"%s","priority":"%s","utilization_pct":%s,"task":"%s"}\n' \
        "$(date -u +%Y-%m-%dT%H:%M:%SZ)" \
        "${provider}" \
        "${agent}" \
        "${priority}" \
        "${utilization}" \
        "$(printf '%s' "${task}" | tr '\n' ' ' | sed 's/"/\\"/g')" \
        >> "${EUXIS_DEFERRED_QUEUE}"
}

_provider_governor_precheck() {
    local provider="$1"
    local agent="$2"
    local task="$3"
    local prompt_tokens="$4"
    local priority="$5"
    local util
    util=$(_projected_utilization_pct "${provider}" "${prompt_tokens}")
    if (( util <= 0 )); then
        if [[ "${EUXIS_OFFPEAK_ONLY_LOW_PRIORITY}" == "1" ]] && _is_low_priority "${priority}" && ! _is_offpeak_now; then
            _enqueue_deferred_task "${provider}" "${agent}" "${priority}" 0 "${task}"
            return 4
        fi
        return 0
    fi

    if (( util >= EUXIS_FAILOVER_THRESHOLD_PCT )); then
        _log_provider_switch "${agent}" "${provider}" "quota_soft_cap"
        return 2
    fi

    if (( util >= EUXIS_THROTTLE_THRESHOLD_PCT )); then
        if _is_low_priority "${priority}"; then
            _enqueue_deferred_task "${provider}" "${agent}" "${priority}" "${util}" "${task}"
            return 3
        fi
        local delay=$(( (util - EUXIS_THROTTLE_THRESHOLD_PCT + 1) * EUXIS_THROTTLE_STEP_SEC ))
        if (( delay > EUXIS_MAX_BACKOFF )); then
            delay="${EUXIS_MAX_BACKOFF}"
        fi
        log_warn "Provider ${provider} utilization at ${util}% — throttling ${delay}s"
        sleep "${delay}"
    fi
    return 0
}

_record_task_profile() {
    local provider="$1"
    local agent="$2"
    local priority="$3"
    local prompt="$4"
    local output="$5"
    local duration_ms="$6"
    local status="$7"
    _ensure_usage_dir
    printf '{"ts":"%s","provider":"%s","agent":"%s","priority":"%s","prompt_tokens":%s,"output_tokens":%s,"duration_ms":%s,"status":"%s"}\n' \
        "$(euxis_ts_utc_ms)" \
        "${provider}" \
        "${agent}" \
        "${priority}" \
        "$(_estimate_tokens "${prompt}")" \
        "$(_estimate_tokens "${output}")" \
        "${duration_ms}" \
        "${status}" \
        >> "${EUXIS_PROVIDER_USAGE_DIR}/task-profiles.jsonl"
}

_record_provider_usage() {
    local provider="$1"
    local input_text="$2"
    local output_text="$3"
    local in_tokens out_tokens total
    in_tokens=$(_estimate_tokens "${input_text}")
    out_tokens=$(_estimate_tokens "${output_text}")
    total=$(( in_tokens + out_tokens ))
    _ensure_usage_dir
    local state window_start used req
    state=$(_read_provider_usage "${provider}")
    IFS='|' read -r window_start used req <<< "${state}"
    used=$(( used + total ))
    req=$(( req + 1 ))
    _write_provider_usage "${provider}" "${window_start}" "${used}" "${req}"
}

_extract_ratelimit_remaining() {
    local output="$1"
    local line
    line=$(printf '%s\n' "${output}" | grep -Eio 'x-ratelimit-remaining[^0-9]*[0-9]+' | tail -n 1 || true)
    if [[ -z "${line}" ]]; then
        echo ""
        return
    fi
    echo "${line##*[!0-9]}"
}

_extract_ratelimit_limit() {
    local output="$1"
    local line
    line=$(printf '%s\n' "${output}" | grep -Eio 'x-ratelimit-limit[^0-9]*[0-9]+' | tail -n 1 || true)
    if [[ -z "${line}" ]]; then
        echo ""
        return
    fi
    echo "${line##*[!0-9]}"
}

_record_ratelimit_hint() {
    local provider="$1"
    local output="$2"
    local remaining limit util
    remaining=$(_extract_ratelimit_remaining "${output}")
    limit=$(_extract_ratelimit_limit "${output}")
    [[ -z "${remaining}" ]] && return 0
    util="null"
    if [[ -n "${limit}" && "${limit}" -gt 0 ]]; then
        util=$(( ((limit - remaining) * 100) / limit ))
    fi
    _ensure_usage_dir
    printf '{"ts":"%s","provider":"%s","remaining":%s,"limit":%s,"utilization_pct":%s}\n' \
        "$(date -u +%Y-%m-%dT%H:%M:%SZ)" \
        "${provider}" \
        "${remaining}" \
        "${limit:-null}" \
        "${util}" \
        >> "${EUXIS_PROVIDER_USAGE_DIR}/ratelimit-hints.jsonl"
}

_latest_ratelimit_utilization_pct() {
    local provider="$1"
    local file="${EUXIS_PROVIDER_USAGE_DIR}/ratelimit-hints.jsonl"
    [[ -f "${file}" ]] || return 1
    jq -r --arg p "${provider}" \
        'select(.provider == $p and (.utilization_pct|type=="number")) | .utilization_pct' \
        "${file}" 2>/dev/null | tail -n 1
}

# Get next fallback provider from chain
# Usage: _get_fallback_provider <current_provider>
_get_fallback_provider() {
    local current="$1"
    local chain="${PROVIDER_FALLBACK_CHAIN}"
    local found_current=0

    IFS=':' read -ra providers <<< "$chain"
    for p in "${providers[@]}"; do
        if [[ $found_current -eq 1 ]]; then
            # Return next provider in chain
            if _provider_available "$p"; then
                echo "$p"
                return 0
            fi
        fi
        if [[ "$p" == "$current" ]]; then
            found_current=1
        fi
    done

    # No fallback available
    return 1
}

# Check if provider is available
# Usage: _provider_available <provider>
_provider_available() {
    local provider="$1"

    case "$provider" in
        claude)
            if ! command -v claude &>/dev/null; then
                return 1
            fi
            if [[ -n "${CLAUDECODE:-}" || -n "${CLAUDE_CODE_ENTRYPOINT:-}" ]]; then
                return 0
            fi
            if [[ -n "${ANTHROPIC_API_KEY:-}" ]]; then
                return 0
            fi
            claude auth status 2>/dev/null | grep -q '"loggedIn":[[:space:]]*true'
            ;;
        gemini)
            command -v gemini &>/dev/null && {
                [[ -n "${GEMINI_API_KEY:-}" || -n "${GOOGLE_API_KEY:-}" ]] || [[ -f "${HOME}/.gemini/oauth_creds.json" ]]
            }
            ;;
        openai)
            if [[ -n "${CODEX_THREAD_ID:-}" ]]; then
                return 0
            fi
            [[ -n "${OPENAI_API_KEY:-}" ]] || command -v codex &>/dev/null
            ;;
        ollama)
            command -v ollama &>/dev/null && ollama list &>/dev/null
            ;;
        qwen)
            command -v qwen &>/dev/null
            ;;
        crush)
            command -v crush &>/dev/null
            ;;
        kiro-cli)
            command -v kiro-cli &>/dev/null
            ;;
        goose)
            command -v goose &>/dev/null && goose --help 2>&1 | grep -q -- '--model'
            ;;
        *)
            command -v "$provider" &>/dev/null
            ;;
    esac
}

# Execute with retry and fallback
# Usage: run_with_fallback <provider> <prompt> <agent> [task]
# Returns: output on success, exits on failure
run_with_fallback() {
    local provider="$1"
    local prompt="$2"
    local agent="${3:-unknown}"
    local task="${4:-}"
    local priority
    priority=$(_resolve_task_priority "${task}")
    local retry=0
    local current_provider="$provider"
    local output=""
    local exit_code=0
    local prompt_tokens
    prompt_tokens=$(_estimate_tokens "${prompt}")
    EUXIS_PROVIDER_INITIAL="${current_provider}"
    EUXIS_PROVIDER_CURRENT="${current_provider}"
    EUXIS_PROVIDER_TRANSITIONS='[]'
    _persist_provider_state

    # Source UI for status display if available
    if [[ -f "${EUXIS_HOME}/euxis-core/lib/core.sh" ]]; then
        source "${EUXIS_HOME}/euxis-core/lib/core.sh" 2>/dev/null || true
    fi

    while true; do
        local plan_hint=""
        if [[ "${agent}" == "architect" || "${agent}" == "planner" ]]; then
            plan_hint=$(_plan_cache_lookup "${agent}" "${task}" 2>/dev/null || true)
        fi
        local effective_prompt="${prompt}"
        if [[ -n "${plan_hint}" ]]; then
            effective_prompt="${prompt}

---
## CACHED PLAN TEMPLATE (APC)
Reuse/adapt this prior successful plan if it fits current context; update paths/dates/entities only as needed.
${plan_hint}"
        fi

        local cached_output=""
        if cached_output=$(_semantic_cache_lookup "${current_provider}" "${agent}" "${effective_prompt}" "${task}" 2>/dev/null); then
            log_info "Semantic cache hit for ${current_provider}/${agent}"
            _record_provider_usage "${current_provider}" "${effective_prompt}" "${cached_output}"
            _record_task_profile "${current_provider}" "${agent}" "${priority}" "${effective_prompt}" "${cached_output}" 1 "cache_hit"
            _maybe_trigger_context_compaction "${current_provider}" "${agent}" "${task}"
            echo "${cached_output}"
            return 0
        fi
        local vector_hit=""
        local threshold
        threshold=$(_vector_threshold_for_task "${task}")
        if vector_hit=$(_semantic_vector_lookup "resp:${current_provider}:${agent}" "${task}" "${threshold}" 2>/dev/null); then
            log_info "Vector semantic cache hit for ${current_provider}/${agent}"
            _record_provider_usage "${current_provider}" "${effective_prompt}" "${vector_hit}"
            _record_task_profile "${current_provider}" "${agent}" "${priority}" "${effective_prompt}" "${vector_hit}" 2 "vector_cache_hit"
            _maybe_trigger_context_compaction "${current_provider}" "${agent}" "${task}"
            echo "${vector_hit}"
            return 0
        fi

        # Check provider availability
        if ! _provider_available "$current_provider"; then
            local prev_provider="$current_provider"
            _log_provider_switch "$agent" "$current_provider" "unavailable"
            current_provider=$(_get_fallback_provider "$current_provider") || {
                log_error "All providers exhausted. No fallback available."
                return 1
            }
            _record_provider_transition "${prev_provider}" "${current_provider}" "unavailable"
            _log_provider_switch "$agent" "$current_provider" "switching"
            continue
        fi

        # Quota governor: throttle, defer low-priority work, and trigger soft-cap failover.
        local gov_code=0
        _provider_governor_precheck "${current_provider}" "${agent}" "${task}" "${prompt_tokens}" "${priority}" || gov_code=$?
        if [[ "${gov_code}" -eq 2 ]]; then
            local prev_provider="$current_provider"
            current_provider=$(_get_fallback_provider "$current_provider") || {
                log_error "Provider ${provider} reached soft cap and no fallback is available."
                return 1
            }
            _record_provider_transition "${prev_provider}" "${current_provider}" "quota_soft_cap"
            _log_provider_switch "$agent" "$current_provider" "switching"
            continue
        elif [[ "${gov_code}" -eq 3 ]]; then
            _log_provider_switch "${agent}" "${current_provider}" "deferred"
            echo "Task deferred due to quota throttling (${current_provider} at/over ${EUXIS_THROTTLE_THRESHOLD_PCT}%)."
            _record_task_profile "${current_provider}" "${agent}" "${priority}" "${prompt}" "" 0 "deferred_quota"
            return 0
        elif [[ "${gov_code}" -eq 4 ]]; then
            _log_provider_switch "${agent}" "${current_provider}" "offpeak_deferred"
            echo "Task deferred until off-peak window for low-priority workload."
            _record_task_profile "${current_provider}" "${agent}" "${priority}" "${prompt}" "" 0 "deferred_offpeak"
            return 0
        fi

        # Resolve provider config
        resolve_provider_config "$current_provider"

        # Execute based on provider
        local _t_exec
        _t_exec=$(_perf_start)
        case "$current_provider" in
            claude)  output=$(run_claude "$effective_prompt" 2>&1); exit_code=$? ;;
            gemini)  output=$(run_gemini "$effective_prompt" 2>&1); exit_code=$? ;;
            openai)  output=$(run_openai "$effective_prompt" 2>&1); exit_code=$? ;;
            ollama)  output=$(run_ollama "$effective_prompt" 2>&1); exit_code=$? ;;
            qwen)    output=$(run_qwen "$effective_prompt" 2>&1); exit_code=$? ;;
            crush)   output=$(run_crush "$effective_prompt" 2>&1); exit_code=$? ;;
            kiro-cli) output=$(run_kiro_cli "$effective_prompt" 2>&1); exit_code=$? ;;
            goose)   output=$(run_goose "$effective_prompt" 2>&1); exit_code=$? ;;
            *)       output=$(run_generic "$current_provider" "$effective_prompt" 2>&1); exit_code=$? ;;
        esac
        local _ms_exec
        _ms_exec=$(_perf_elapsed_ms "${_t_exec}")

        if [[ $exit_code -eq 0 ]]; then
            _record_provider_usage "${current_provider}" "${effective_prompt}" "${output}"
            _record_ratelimit_hint "${current_provider}" "${output}"
            _semantic_cache_store "${current_provider}" "${agent}" "${effective_prompt}" "${task}" "${output}"
            _semantic_vector_store "resp:${current_provider}:${agent}" "${task}" "${output}"
            _plan_cache_store "${agent}" "${task}" "${output}"
            _record_task_profile "${current_provider}" "${agent}" "${priority}" "${effective_prompt}" "${output}" "${_ms_exec}" "success"
            _maybe_trigger_context_compaction "${current_provider}" "${agent}" "${task}"
            echo "$output"
            return 0
        fi
        _record_task_profile "${current_provider}" "${agent}" "${priority}" "${effective_prompt}" "${output}" "${_ms_exec}" "error"

        # Check if error is retryable
        if _is_retryable_error "$output"; then
            retry=$((retry + 1))
            if [[ $retry -le ${EUXIS_MAX_RETRIES} ]]; then
                _log_retry "$agent" "$current_provider" "$retry"
                local backoff=$(( EUXIS_RETRY_BACKOFF * (2 ** (retry - 1)) ))
                if (( backoff > EUXIS_MAX_BACKOFF )); then
                    backoff="${EUXIS_MAX_BACKOFF}"
                fi
                sleep "${backoff}"
                continue
            fi
        fi

        # Try fallback provider
        local fallback
        fallback=$(_get_fallback_provider "$current_provider") || {
            log_error "Provider $current_provider failed and no fallback available"
            echo "$output"
            return $exit_code
        }

        _record_provider_transition "${current_provider}" "${fallback}" "provider_failure"
        _log_provider_switch "$agent" "$fallback" "failover"
        current_provider="$fallback"
        retry=0
    done
}

# Check if error is retryable (timeout, rate limit, temporary)
_is_retryable_error() {
    local error="$1"

    # Retryable patterns
    [[ "$error" == *"timeout"* ]] && return 0
    [[ "$error" == *"rate limit"* ]] && return 0
    [[ "$error" == *"429"* ]] && return 0
    [[ "$error" == *"503"* ]] && return 0
    [[ "$error" == *"temporary"* ]] && return 0
    [[ "$error" == *"connection"* ]] && return 0

    return 1
}

# Log provider switch for UX
_log_provider_switch() {
    local agent="$1"
    local provider="$2"
    local reason="$3"

    local icon color msg
    case "$reason" in
        unavailable)
            icon="✖"; color="\033[38;5;203m"
            msg="[Provider unavailable]"
            ;;
        switching)
            icon="⚠"; color="\033[38;5;220m"
            msg="[Switching to $provider]"
            ;;
        failover)
            icon="⚠"; color="\033[38;5;220m"
            msg="[Failover to $provider]"
            ;;
        quota_soft_cap)
            icon="⚠"; color="\033[38;5;220m"
            msg="[Quota soft-cap reached on $provider]"
            ;;
        deferred)
            icon="⚠"; color="\033[38;5;220m"
            msg="[Deferred due to quota throttling]"
            ;;
        offpeak_deferred)
            icon="⚠"; color="\033[38;5;220m"
            msg="[Deferred to off-peak window]"
            ;;
        *)
            icon="⚠"; color="\033[38;5;220m"
            msg="[Provider event: $reason]"
            ;;
    esac

    echo -e "${color}${icon}\033[0m ${agent}  ${msg}" >&2
}

# Log retry attempt
_log_retry() {
    local agent="$1"
    local provider="$2"
    local attempt="$3"

    echo -e "\033[38;5;220m⚠\033[0m ${agent}  [Retry $attempt with backoff...]" >&2
}

# ============================================================================
# Intelligence Tiering (v4.7)
# Maps agent -> optimal provider based on task complexity profile.
# Explicit provider argument always overrides tiering.
# P0 tasks always use Strategic tier (claude) regardless of agent.
# ============================================================================

resolve_tiered_provider() {
    local agent="$1"
    local priority="${2:-}"
    if [[ -n "${EUXIS_FORCE_PROVIDER:-}" ]]; then
        echo "${EUXIS_FORCE_PROVIDER}"
        return
    fi
    # P0 tasks ALWAYS use Strategic tier (claude) regardless of agent
    if [[ "$priority" == "P0" ]]; then
        echo "claude"
        return
    fi
    case "${agent}" in
        # S-Tier: Strategic — best-in-class reasoning and tool use
        orchestrator|architect|product-manager|reviewer)
            echo "claude" ;;
        # A-Tier: Research — massive context window for deep analysis + web search
        researcher|deep-researcher|compliance-officer)
            echo "gemini" ;;
        # A-Tier: Enterprise — corporate security contexts
        incident-commander)
            echo "kiro-cli" ;;
        # B-Tier: Coding — agent-native tool use for developer workflows
        bug-fixer|unit-tester|automation-engineer)
            echo "goose" ;;
        # B-Tier: Local Code — specialized local models for diffs and migrations
        legacy-maintainer)
            echo "goose" ;;
        # A-Tier: Domain Specialists — require deep reasoning for domain correctness
        crypto-cryptography-auditor|payments-domain-steward)
            echo "claude" ;;
        # B-Tier: Realtime/Systems — specialized tool use for systems work
        realtime-audio-engineer|rust-crate-steward)
            echo "goose" ;;
        # B-Tier: Math/Logic — strong at algorithmic and optimization tasks
        perf-optimizer|data-steward)
            echo "qwen" ;;
        # C-Tier: Utility — zero latency, no cost for summaries and formatting
        butler|librarian|tech-writer)
            echo "ollama" ;;
        # Default (Standard): fall back to primary provider
        *)
            echo "${DEFAULT_PROVIDER}" ;;
    esac
}

# Validate model name against safe character allowlist.
# Allows: alphanumeric, dash, dot, slash, underscore, colon (covers all
# known model naming conventions: org/model, model:tag, model-version).
_validate_model_name() {
    local model="$1"
    if [[ ! "${model}" =~ ^[a-zA-Z0-9._/:@-]+$ ]]; then
        log_error "Invalid model name: '${model}' — contains disallowed characters"
        exit 1
    fi
    if [[ "${#model}" -gt 128 ]]; then
        log_error "Invalid model name: '${model}' — exceeds 128 character limit"
        exit 1
    fi
}

# Resolve provider-specific model name and CLI flags.
# Populates: PROVIDER_MODEL, PROVIDER_FLAGS
resolve_provider_config() {
    local provider="$1"

    case "${provider}" in
        claude)   # The Strategist (Primary)
            PROVIDER_MODEL="claude-sonnet-4-20250514"
            PROVIDER_FLAGS="--cache-prompt --stream"
            ;;
        gemini)   # The Researcher (Massive Context, built-in search grounding)
            PROVIDER_MODEL="gemini-2.0-flash"
            PROVIDER_FLAGS=""
            ;;
        openai)   # The Generalist
            PROVIDER_MODEL="${EUXIS_OPENAI_MODEL:-gpt-4o}"
            PROVIDER_FLAGS="--stream"
            ;;
        ollama)   # The Local Assistant (Fast, zero latency)
            PROVIDER_MODEL="${EUXIS_OLLAMA_MODEL:-llama3.2}"
            PROVIDER_FLAGS="--local"
            ;;
        qwen)     # Alibaba Qwen3-Coder (Open Source, 256K context)
            PROVIDER_MODEL="${EUXIS_QWEN_MODEL:-qwen3-coder}"
            PROVIDER_FLAGS=""
            ;;
        crush)    # Charm Crush (Multi-model TUI agent)
            PROVIDER_MODEL="${EUXIS_CRUSH_MODEL:-claude-sonnet-4}"
            PROVIDER_FLAGS=""
            ;;
        kiro-cli) # Kiro CLI (AI coding assistant)
            PROVIDER_MODEL="kiro-cli"
            PROVIDER_FLAGS=""
            ;;
        goose)    # Block Goose (Open source, MCP-native agent)
            PROVIDER_MODEL="${EUXIS_GOOSE_MODEL:-claude-sonnet-4}"
            ;;
        *)
            log_error "Unknown provider: '${provider}'"
            exit 1
            ;;
    esac

    # Validate model name (especially important for env-var-sourced values)
    _validate_model_name "${PROVIDER_MODEL}"
}

# Wrapper function to add configurable timeout to provider commands
run_with_timeout() {
    local timeout_seconds="$1"
    shift
    local command_description="$1"
    shift

    if command -v timeout &>/dev/null; then
        # GNU timeout available (Linux, installed via brew on macOS)
        timeout "${timeout_seconds}s" "$@"
        local exit_code=$?
        if [[ $exit_code -eq 124 ]]; then
            log_error "Provider command timed out after ${timeout_seconds}s: ${command_description}"
            log_error "Consider increasing EUXIS_API_TIMEOUT or using a different provider"
        fi
        return $exit_code
    elif command -v gtimeout &>/dev/null; then
        # GNU timeout via coreutils on macOS
        gtimeout "${timeout_seconds}s" "$@"
        local exit_code=$?
        if [[ $exit_code -eq 124 ]]; then
            log_error "Provider command timed out after ${timeout_seconds}s: ${command_description}"
            log_error "Consider increasing EUXIS_API_TIMEOUT or using a different provider"
        fi
        return $exit_code
    else
        # No timeout command available - run without timeout but log warning
        log_warn "timeout command not available - running without timeout protection"
        "$@"
    fi
}

run_claude() {
    local full_prompt="$1"

    # Check for optional dependencies and adjust tools accordingly
    local allowed_tools="Read,Edit,Write,Bash(grep:*) Bash(find:*) Bash(python3:*) Bash(pytest:*) Bash(cat:*) Bash(ls:*) Bash(test:*) Bash(head:*) Bash(tail:*) Bash(wc:*) Bash(mkdir:*) Bash(touch:*) Bash(pip:*) Bash(uv:*) Bash(curl:*) Bash(git:*) Bash(sed:*) Bash(awk:*) Bash(cp:*) Bash(mv:*) Bash(npm:*) Bash(npx:*) Bash(cargo:*) Bash(make:*) Bash(chmod:*) Bash(tar:*) Bash(zip:*) Bash(unzip:*) Bash(docker:*) Bash(diff:*) Bash(sort:*) Bash(tee:*) Bash(xargs:*)"

    # Add jq support if available, otherwise warn
    if command -v jq &>/dev/null; then
        allowed_tools="${allowed_tools} Bash(jq:*)"
    else
        log_warn "jq not available - JSON processing may be limited"
    fi

    # Add gh support if available, otherwise warn
    if command -v gh &>/dev/null; then
        allowed_tools="${allowed_tools} Bash(gh:*)"
    else
        log_warn "gh CLI not available - GitHub operations may be limited"
    fi

    # Use timeout wrapper for the claude command
    # Tools include WebSearch and WebFetch for external research capability
    # Default max turns: 50 for complex analysis tasks, configurable via EUXIS_MAX_TURNS
    # Always strip CLAUDECODE/CLAUDE_CODE_ENTRYPOINT to prevent nested session errors.
    # Claude Code rejects launches inside an existing session; env -u ensures clean subprocess.
    printf '%s\n' "${full_prompt}" | run_with_timeout "${EUXIS_API_TIMEOUT}" "claude API" \
        env -u CLAUDECODE -u CLAUDE_CODE_ENTRYPOINT \
        claude \
        --print \
        --model "${PROVIDER_MODEL}" \
        --tools "Read,Edit,Write,Bash,WebSearch,WebFetch" \
        --allowedTools "${allowed_tools}" \
        --max-turns "${EUXIS_MAX_TURNS:-50}"
}

run_gemini() {
    local full_prompt="$1"
    if command -v gemini &>/dev/null; then
        local gemini_home gemini_config
        gemini_home="${EUXIS_GEMINI_HOME:-/tmp/euxis-gemini-home}"
        gemini_config="${gemini_home}/.gemini"

        if [[ ! -d "${gemini_config}" ]]; then
            umask 077
            mkdir -p "${gemini_config}/tmp" "${gemini_config}/policies"
            if [[ -d "${HOME}/.gemini" ]]; then
                cp -a "${HOME}/.gemini/oauth_creds.json" "${gemini_config}/" 2>/dev/null || true
                cp -a "${HOME}/.gemini/google_accounts.json" "${gemini_config}/" 2>/dev/null || true
                cp -a "${HOME}/.gemini/settings.json" "${gemini_config}/" 2>/dev/null || true
                cp -a "${HOME}/.gemini/state.json" "${gemini_config}/" 2>/dev/null || true
                cp -a "${HOME}/.gemini/installation_id" "${gemini_config}/" 2>/dev/null || true
                cp -a "${HOME}/.gemini/trustedFolders.json" "${gemini_config}/" 2>/dev/null || true
                if [[ -d "${HOME}/.gemini/policies" ]]; then
                    cp -a "${HOME}/.gemini/policies/." "${gemini_config}/policies/" 2>/dev/null || true
                fi
            fi
        fi

        local guard
        guard="IMPORTANT: This is plain-text mode. You do not have structured tool access, but you CAN and SHOULD output complete file contents in fenced code blocks with file paths. Use markdown code blocks with the filename as a comment on the first line. When implementation requires creating files, output the full file contents so they can be applied."
        # SECURITY: Use private temp directory to prevent race conditions (S4-002 fix)
        local err_file secure_tmpdir
        secure_tmpdir="${XDG_RUNTIME_DIR:-${TMPDIR:-/tmp}}/euxis-$$"
        mkdir -p "$secure_tmpdir" 2>/dev/null
        chmod 700 "$secure_tmpdir" 2>/dev/null
        err_file="${secure_tmpdir}/gemini_err"
        touch "$err_file" 2>/dev/null && chmod 600 "$err_file" 2>/dev/null
        if [[ ! -w "$err_file" ]]; then
            echo "[euxis] Error: failed to create secure temp file" >&2
            return 1
        fi
        # Cleanup on exit
        trap "rm -rf '$secure_tmpdir' 2>/dev/null" EXIT
        local out prompt
        prompt=$(printf '%s\n\n%s' "${guard}" "${full_prompt}")
        # shellcheck disable=SC2086
        if command -v mise &>/dev/null; then
            out=$(HOME="${gemini_home}" NODE_OPTIONS="--no-deprecation" mise exec -- gemini ${PROVIDER_FLAGS} --output-format text -p "${prompt}" 2> "${err_file}")
        else
            out=$(HOME="${gemini_home}" NODE_OPTIONS="--no-deprecation" gemini ${PROVIDER_FLAGS} --output-format text -p "${prompt}" 2> "${err_file}")
        fi
        local rc=$?
        if [[ -s "${err_file}" ]]; then
            grep -vE '^(Error executing tool|Tool execution denied by policy|Tool ".*" not found in registry|Loaded cached credentials|Hook registry initialized|\\(node:.*\\) \\[DEP0040\\])' "${err_file}" >&2 || true
        fi
        rm -f "${err_file}"
        printf '%s' "${out}" | grep -vE '^(Error executing tool|Tool execution denied by policy|Tool ".*" not found in registry|Loaded cached credentials|Hook registry initialized)$' || true
        return "${rc}"
    else
        log_error "gemini CLI not found — install via: npm i -g @anthropic-ai/gemini-cli"
        exit 1
    fi
}

run_openai() {
    local full_prompt="$1"
    if command -v codex &>/dev/null; then
        local session_active=0
        if [[ "${EUXIS_PRESERVE_PROVIDER_SESSION}" == "1" ]] && \
            [[ -n "${CODEX_THREAD_ID:-}" || -n "${CODEX_CI:-}" || -n "${CODEX_SESSION_ID:-}" ]]; then
            session_active=1
        fi
        if [[ "${session_active}" -eq 1 ]]; then
            printf '%s\n' "${full_prompt}" | run_with_timeout "${EUXIS_API_TIMEOUT}" "codex API" codex exec -
        elif [[ -z "${PROVIDER_MODEL}" || "${PROVIDER_MODEL}" == "auto" ]]; then
            printf '%s\n' "${full_prompt}" | run_with_timeout "${EUXIS_API_TIMEOUT}" "codex API" codex exec -
        else
            printf '%s\n' "${full_prompt}" | run_with_timeout "${EUXIS_API_TIMEOUT}" "codex API" codex exec --model "${PROVIDER_MODEL}" -
        fi
    else
        log_error "codex CLI not found — install via: npm i -g @openai/codex"
        exit 1
    fi
}

run_ollama() {
    local full_prompt="$1"
    if command -v ollama &>/dev/null; then
        printf '%s\n' "${full_prompt}" | run_with_timeout "${EUXIS_API_TIMEOUT}" "ollama API" ollama run "${PROVIDER_MODEL}"
    else
        log_error "ollama not found — install via: brew install ollama"
        exit 1
    fi
}


run_qwen() {
    local full_prompt="$1"
    if command -v qwen &>/dev/null; then
        printf '%s\n' "${full_prompt}" | run_with_timeout "${EUXIS_API_TIMEOUT}" "qwen API" qwen --model "${PROVIDER_MODEL}" -p ""
    else
        log_error "qwen not found — install via: brew install qwen-code"
        exit 1
    fi
}

run_crush() {
    local full_prompt="$1"
    if command -v crush &>/dev/null; then
        printf '%s\n' "${full_prompt}" | run_with_timeout "${EUXIS_API_TIMEOUT}" "crush API" crush --model "${PROVIDER_MODEL}"
    else
        log_error "crush not found — install via: brew install charmbracelet/tap/crush"
        exit 1
    fi
}


run_kiro_cli() {
    local full_prompt="$1"
    if command -v kiro-cli &>/dev/null; then
        printf '%s\n' "${full_prompt}" | run_with_timeout "${EUXIS_API_TIMEOUT}" "kiro-cli API" kiro-cli chat
    else
        log_error "kiro-cli not found — install via: npm i -g kiro-cli"
        exit 1
    fi
}

run_goose() {
    local full_prompt="$1"
    if command -v goose &>/dev/null; then
        printf '%s\n' "${full_prompt}" | run_with_timeout "${EUXIS_API_TIMEOUT}" "goose API" goose run --model "${PROVIDER_MODEL}"
    else
        log_error "goose not found — install via: brew install block/tap/goose"
        exit 1
    fi
}

# ============================================================================
# Router-Aware Execution (v0.0.2)
# Uses router.sh for cost-optimized model selection when available.
# Falls back to tiered provider selection if router unavailable.
# ============================================================================

# Execute with router-optimized model selection
# Usage: execute_with_router <agent_id> <task> <full_prompt>
execute_with_router() {
    local agent_id="$1"
    local task="$2"
    local full_prompt="$3"

    # Use router for model selection if available
    if [[ "${ROUTER_INTEGRATED:-false}" == "true" ]]; then
        # Check for local model fallback (zero cost)
        if router_should_use_local "$task" 2>/dev/null; then
            local local_model
            local_model=$(router_local_model)
            log_info "Using local model: ${local_model} (zero cost)"
            PROVIDER_MODEL="$local_model"
            run_ollama "$full_prompt"
            return $?
        fi

        # Use router for optimal model selection
        local selected_model
        selected_model=$(router_select_model "$agent_id" "$task" 2>/dev/null)

        if [[ -n "$selected_model" ]]; then
            # Map model to provider
            local provider
            case "$selected_model" in
                *opus*|*sonnet*)
                    provider="claude"
                    ;;
                *gemini*|*flash*)
                    provider="gemini"
                    ;;
                *deepseek*|*qwen*)
                    provider="ollama"
                    selected_model="${EUXIS_OLLAMA_MODEL:-$selected_model}"
                    ;;
                *gpt*)
                    provider="openai"
                    ;;
                *)
                    provider="claude"
                    ;;
            esac

            # Override the model if router selected one
            if [[ -n "${EUXIS_MODEL_OVERRIDE:-}" ]]; then
                selected_model="$EUXIS_MODEL_OVERRIDE"
            fi

            log_info "Router selected: ${selected_model} via ${provider}"
            PROVIDER_MODEL="$selected_model"
            execute_provider "$provider" "$full_prompt"
            return $?
        fi
    fi

    # Fallback to tiered provider selection
    local provider
    provider=$(resolve_tiered_provider "$agent_id")
    execute_provider "$provider" "$full_prompt"
}

execute_provider() {
    local provider="$1"
    local full_prompt="$2"

    case "${provider}" in
        claude|gemini|openai|ollama|qwen|crush|kiro-cli|goose) ;;
        *)
            log_error "Unknown provider: '${provider}'"
            exit 1
            ;;
    esac

    # Mock mode for testing (no API calls)
    if [[ "${EUXIS_MOCK_PROVIDER:-}" == "true" ]]; then
        echo "[MOCK] Provider: ${provider}, Model: ${PROVIDER_MODEL:-unknown}"
        echo "[MOCK] Prompt received (${#full_prompt} chars)"
        return 0
    fi

    # Resolve model and flags before execution
    resolve_provider_config "${provider}"

    case "${provider}" in
        claude)    run_claude "${full_prompt}" ;;
        gemini)    run_gemini "${full_prompt}" ;;
        openai)    run_openai "${full_prompt}" ;;
        ollama)    run_ollama "${full_prompt}" ;;
        qwen)      run_qwen "${full_prompt}" ;;
        crush)     run_crush "${full_prompt}" ;;
        kiro-cli)  run_kiro_cli "${full_prompt}" ;;
        goose)     run_goose "${full_prompt}" ;;
    esac
}
