#!/usr/bin/env bash
# GitHub PR Tracker - Main display entry point
# Fetches open PR data and prints Waybar JSON via python module

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [[ -f "$SCRIPT_DIR/.env" ]]; then
    set -a
    source "$SCRIPT_DIR/.env"
    set +a
fi

export CACHE_DIR="${CACHE_DIR:-${XDG_CACHE_HOME:-$HOME/.cache}/github-pr-tracker}"
API_BASE="${API_BASE:-https://api.github.com}"
GITHUB_USERNAME="${GITHUB_USERNAME:-}"
PER_PAGE="${PER_PAGE:-30}"
CACHE_TTL="${CACHE_TTL:-60}"
CURL_TIMEOUT="${CURL_TIMEOUT:-10}"
INVOLVED_QUERY="${INVOLVED_QUERY:-is:pr is:open archived:false involves:${GITHUB_USERNAME}}"
REVIEW_QUERY="${REVIEW_QUERY:-is:pr is:open archived:false review-requested:${GITHUB_USERNAME}}"

INVOLVED_CACHE="$CACHE_DIR/github-pr-involved.json"
REVIEW_CACHE="$CACHE_DIR/github-pr-review.json"
mkdir -p "$CACHE_DIR"

# URL-encode the GitHub search query safely.
urlencode() {
    python3 -c 'import sys, urllib.parse; print(urllib.parse.quote(sys.argv[1]))' "$1"
}

build_search_url() {
    local query="$1"
    local encoded
    encoded="$(urlencode "$query")"
    echo "$API_BASE/search/issues?q=$encoded&sort=updated&order=desc&per_page=$PER_PAGE"
}

CURL_OPTS=(
    -s
    -m "$CURL_TIMEOUT"
    -H "Accept: application/vnd.github+json"
    -H "X-GitHub-Api-Version: 2022-11-28"
    -H "User-Agent: waybar-github-pr-tracker"
)

if [[ -n "${GITHUB_TOKEN:-}" ]]; then
    CURL_OPTS+=( -H "Authorization: Bearer $GITHUB_TOKEN" )
fi

fetch_api() {
    local endpoint="$1"
    local cache="$2"
    local ttl="$3"

    if [[ -f "$cache" ]]; then
        local age=$(( $(date +%s) - $(stat -c%Y "$cache" 2>/dev/null || echo 0) ))
        if (( age < ttl )); then
            cat "$cache"
            return 0
        fi
    fi

    local resp
    resp="$(curl "${CURL_OPTS[@]}" "$endpoint" 2>/dev/null || echo "{}")"
    if [[ -z "$resp" ]]; then
        resp='{}'
    fi
    printf '%s' "$resp" > "$cache"
    printf '%s' "$resp"
}

if [[ -z "$GITHUB_USERNAME" ]]; then
    printf '{"items":[]}' > "$INVOLVED_CACHE"
    printf '{"items":[]}' > "$REVIEW_CACHE"
else
    INVOLVED_ENDPOINT="$(build_search_url "$INVOLVED_QUERY")"
    REVIEW_ENDPOINT="$(build_search_url "$REVIEW_QUERY")"

    fetch_api "$INVOLVED_ENDPOINT" "$INVOLVED_CACHE" "$CACHE_TTL" >/dev/null &
    fetch_api "$REVIEW_ENDPOINT" "$REVIEW_CACHE" "$CACHE_TTL" >/dev/null &
    wait
fi

CACHE_DIR="$CACHE_DIR" python3 "$SCRIPT_DIR/github_pr_module.py"
