#!/usr/bin/env bash
# Enhanced LCK Scores Fetcher with detailed information
# Shows full match details with team records and game state

set -euo pipefail

CACHE_DIR="${XDG_CACHE_HOME:-$HOME/.cache}/lck-scores"
CACHE_FILE="$CACHE_DIR/lck-data-detailed.json"
CACHE_TTL=60

mkdir -p "$CACHE_DIR"

# Get cached data if fresh
get_cached_data() {
    if [[ -f "$CACHE_FILE" ]]; then
        local file_age=$(( $(date +%s) - $(stat -f%m "$CACHE_FILE" 2>/dev/null || stat -c%Y "$CACHE_FILE") ))
        if [[ $file_age -lt $CACHE_TTL ]]; then
            cat "$CACHE_FILE"
            return 0
        fi
    fi
    return 1
}

# Fetch schedule data
fetch_schedule() {
    if get_cached_data; then
        return 0
    fi

    local response
    response=$(curl -s -m 10 \
        -H "User-Agent: Mozilla/5.0" \
        "https://esports-api.lolesports.com/persisted/gw/getSchedule?hl=en-US" 2>/dev/null || echo "{}")

    echo "$response" > "$CACHE_FILE"
    echo "$response"
}

# Parse and display detailed LCK information
display_lck_detailed() {
    local json_data="$1"

    echo "╔════════════════════════════════════════╗"
    echo "║     LCK (League Champions Korea)       ║"
    echo "╚════════════════════════════════════════╝"
    echo ""

    local found_games=false

    # Find matches
    echo "$json_data" | jq -r '.data.schedule[]? | 
        select(.league == "lck") |
        select(.status != "completed") |
        "\(.team1.code)|\(.team2.code)|\(.status)|\(.begin_at)|\(.team1_result.game_count // 0)|\(.team2_result.game_count // 0)"' 2>/dev/null | while IFS='|' read -r team1 team2 status begin_at score1 score2; do
        found_games=true
        
        echo "Team 1: $team1 | Team 2: $team2"
        echo "Score: $score1 - $score2"
        echo "Status: $status"
        echo "Begin: $begin_at"
        echo "────────────────────────────────────────"
    done

    if ! $found_games; then
        echo "No active LCK matches at this time."
    fi
}

# Main
main() {
    local json_data
    json_data=$(fetch_schedule)

    if [[ -z "$json_data" || "$json_data" == "{}" ]]; then
        echo "Failed to fetch LCK data"
        exit 1
    fi

    display_lck_detailed "$json_data"
}

main
