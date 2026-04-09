#!/usr/bin/env bash
# LoL Detailed Scores Fetcher
# Shows full match details with team records and game state across all regions

set -euo pipefail

CACHE_DIR="${XDG_CACHE_HOME:-$HOME/.cache}/lol-scores"
CACHE_FILE="$CACHE_DIR/lol-data-detailed.json"
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

# Parse and display detailed LoL Esports information
display_lol_detailed() {
    local json_data="$1"

    echo "╔════════════════════════════════════════╗"
    echo "║         LoL Esports - Detailed         ║"
    echo "╚════════════════════════════════════════╝"
    echo ""

    local found_games=false

    # Find matches (generic across leagues) - adapt jq to API structure
    echo "$json_data" | jq -r '.data.schedule.events[]? |
        select(.match != null) |
        select(.state != "completed") |
        "\(.league.name)//\(.match.teams[0].code)//\(.match.teams[1].code)//\(.state)//\(.startTime)//\(.match.teams[0].result.gameWins // 0)//\(.match.teams[1].result.gameWins // 0)"' 2>/dev/null | while IFS='//' read -r league team1 team2 state begin_at score1 score2; do
        found_games=true

        echo "League: $league"
        echo "Team 1: $team1 | Team 2: $team2"
        echo "Score: $score1 - $score2"
        echo "State: $state"
        echo "Start: $begin_at"
        echo "────────────────────────────────────────"
    done

    if ! $found_games; then
        echo "No active matches at this time."
    fi
}

# Main
main() {
    local json_data
    json_data=$(fetch_schedule)

    if [[ -z "$json_data" || "$json_data" == "{}" ]]; then
        echo "Failed to fetch data"
        exit 1
    fi

    display_lol_detailed "$json_data"
}

main
