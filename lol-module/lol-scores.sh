#!/usr/bin/env bash
# LoL Esports Scores Display - Main entry point
# Fetches live and upcoming match data for all LoL Esports regions and displays via Waybar

set -euo pipefail

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Load environment configuration
if [[ -f "$SCRIPT_DIR/.env" ]]; then
    set -a
    source "$SCRIPT_DIR/.env"
    set +a
fi

# Set defaults if not in .env
export CACHE_DIR="${CACHE_DIR:-${XDG_CACHE_HOME:-$HOME/.cache}/lol-scores}"
API_BASE="${API_BASE:-https://esports-api.lolesports.com/persisted/gw}"
LIVE_ENDPOINT="${LIVE_ENDPOINT:-$API_BASE/getLive?hl=en-US}"
SCHEDULE_ENDPOINT="${SCHEDULE_ENDPOINT:-$API_BASE/getSchedule?hl=en-US}"
# Require API_KEY from .env or environment — no hardcoded fallback for security
API_KEY="${API_KEY:-}"
LIVE_CACHE_TTL="${LIVE_CACHE_TTL:-30}"
SCHEDULE_CACHE_TTL="${SCHEDULE_CACHE_TTL:-60}"
CURL_TIMEOUT="${CURL_TIMEOUT:-10}"

LIVE_CACHE="$CACHE_DIR/lol-live-games.json"
SCHEDULE_CACHE="$CACHE_DIR/lol-data.json"
mkdir -p "$CACHE_DIR"

if [[ -z "$API_KEY" ]]; then
    echo "lol-scores: WARNING — API_KEY not set. API requests may be rate-limited." >&2
fi

# Build curl options
CURL_OPTS=( 
    -s 
    -m "$CURL_TIMEOUT"
    -H "User-Agent: Mozilla/5.0"
    -H "Accept: application/json"
    -H "Referer: https://www.lolesports.com"
    -H "Origin: https://www.lolesports.com"
)

if [[ -n "$API_KEY" ]]; then
    CURL_OPTS+=( 
        -H "Authorization: Bearer $API_KEY"
        -H "x-api-key: $API_KEY"
    )
fi

# Fetch and cache API data
fetch_api() {
    local endpoint=$1 cache=$2 ttl=$3
    
    # Return cached data if fresh
    if [[ -f "$cache" ]]; then
        local age=$(( $(date +%s) - $(stat -c%Y "$cache" 2>/dev/null || echo 0) ))
        if (( age < ttl )); then
            cat "$cache"
            return 0
        fi
    fi
    
    # Fetch fresh data
    local resp=$(curl "${CURL_OPTS[@]}" "$endpoint" 2>/dev/null || echo "{}")
    printf '%s' "$resp" > "$cache"
    printf '%s' "$resp"
}

# Fetch data in parallel
fetch_api "$LIVE_ENDPOINT" "$LIVE_CACHE" "$LIVE_CACHE_TTL" >/dev/null &
fetch_api "$SCHEDULE_ENDPOINT" "$SCHEDULE_CACHE" "$SCHEDULE_CACHE_TTL" >/dev/null &
wait

# Run Python module to display with explicit environment
CACHE_DIR="$CACHE_DIR" python3 "$SCRIPT_DIR/lol_module.py"
