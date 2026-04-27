#!/usr/bin/env bash
# Waybar anime module entrypoint.
# Fetches MAL watching list and delegates rendering to anime_module.py.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [[ -f "$SCRIPT_DIR/.env" ]]; then
    set -a
    source "$SCRIPT_DIR/.env"
    set +a
fi

emit_hidden() {
    printf '{"text":"","tooltip":"","class":"hidden"}\n'
}

MAL_API_BASE="${MAL_API_BASE:-https://api.myanimelist.net/v2}"
CACHE_DIR="${CACHE_DIR:-${XDG_CACHE_HOME:-$HOME/.cache}/anime-module}"
MAL_CACHE_TTL="${MAL_CACHE_TTL:-900}"
CURL_TIMEOUT="${CURL_TIMEOUT:-15}"
MAL_USERNAME="${MAL_USERNAME:-@me}"
ANIMELIST_CACHE="$CACHE_DIR/mal-watching.json"

mkdir -p "$CACHE_DIR"

is_cache_fresh() {
    local file="$1"
    local ttl="$2"

    if [[ ! -f "$file" ]]; then
        return 1
    fi

    local now mtime age
    now="$(date +%s)"
    mtime="$(stat -c %Y "$file" 2>/dev/null || echo 0)"
    age=$((now - mtime))
    (( age < ttl ))
}

CURL_HEADERS=(
    -H "Accept: application/json"
    -H "User-Agent: waybar-anime-module/1.0"
)

if [[ -n "${MAL_ACCESS_TOKEN:-}" ]]; then
    CURL_HEADERS+=( -H "Authorization: Bearer ${MAL_ACCESS_TOKEN}" )
fi

if [[ -n "${MAL_CLIENT_ID:-}" ]]; then
    CURL_HEADERS+=( -H "X-MAL-CLIENT-ID: ${MAL_CLIENT_ID}" )
fi

if [[ -z "${MAL_ACCESS_TOKEN:-}" && -z "${MAL_CLIENT_ID:-}" ]]; then
    echo "anime-module: MAL_ACCESS_TOKEN or MAL_CLIENT_ID is required" >&2
    emit_hidden
    exit 0
fi

if [[ -z "${MAL_ACCESS_TOKEN:-}" && "$MAL_USERNAME" == "@me" ]]; then
    echo "anime-module: MAL_USERNAME must be set when using MAL_CLIENT_ID without token" >&2
    emit_hidden
    exit 0
fi

REQUEST_URL="${MAL_API_BASE}/users/${MAL_USERNAME}/animelist?status=watching&sort=anime_title&limit=1000&fields=list_status,num_episodes,broadcast,status,start_date"

if ! is_cache_fresh "$ANIMELIST_CACHE" "$MAL_CACHE_TTL"; then
    response="$(curl -sS -m "$CURL_TIMEOUT" "${CURL_HEADERS[@]}" "$REQUEST_URL" 2>/dev/null || true)"

    if [[ -n "$response" ]] && printf '%s' "$response" | python3 -c 'import json,sys; obj=json.load(sys.stdin); sys.exit(0 if isinstance(obj.get("data"), list) else 1)' >/dev/null 2>&1; then
        printf '%s' "$response" > "$ANIMELIST_CACHE"
    elif [[ ! -f "$ANIMELIST_CACHE" ]]; then
        echo "anime-module: unable to fetch MAL animelist and no cache is available" >&2
        emit_hidden
        exit 0
    fi
fi

CACHE_DIR="$CACHE_DIR" ANIMELIST_CACHE="$ANIMELIST_CACHE" MAX_TITLE_LEN="${MAX_TITLE_LEN:-28}" python3 "$SCRIPT_DIR/anime_module.py"
