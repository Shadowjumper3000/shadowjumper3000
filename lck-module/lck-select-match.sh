#!/usr/bin/env bash
# LCK Match Selector
# Displays a menu to select which live match to display when multiple are running
# Called by right-click action in waybar

set -euo pipefail

CACHE_DIR="${XDG_CACHE_HOME:-$HOME/.cache}/lck-scores"
LIVE_CACHE="$CACHE_DIR/lck-data.json"
LIVE_API_CACHE="$CACHE_DIR/lck-live-games.json"
SELECTION_FILE="$CACHE_DIR/selected-match.txt"

mkdir -p "$CACHE_DIR"

# Get all live LCK games
get_live_matches() {
    if [[ ! -f "$LIVE_CACHE" ]]; then
        return 1
    fi
    
    local json_data=$(cat "$LIVE_CACHE")
    
        echo "$json_data" | jq -r '
            (.data.schedule.events[]? // .data.schedule[]? // []) as $e |
            ($e.match.id // $e.match_id // $e.id // "") as $match_id |
            ($e.match.teams[0].code // $e.team1.code // "") as $t1 |
            ($e.match.teams[1].code // $e.team2.code // "") as $t2 |
            ($e.state // $e.status // "") as $status |
            ($e.league.name // $e.league.slug // $e.league // "") as $league |
            select((($league|ascii_downcase) | contains("lck"))) |
            select((($status|ascii_downcase) | contains("inprog"))) |
            "\($match_id)|\($t1)|\($t2)"
        ' 2>/dev/null || true
}

# Get kill counts for a match
get_match_kills() {
    local team1="$1"
    local team2="$2"
    
    if [[ ! -f "$LIVE_API_CACHE" ]]; then
        return 1
    fi
    
    local live_data=$(cat "$LIVE_API_CACHE")
    
        echo "$live_data" | jq -r '
            (.data.esports.events[]? // .data.schedule.events[]? // []) |
            select((.tournament.region // .league.slug // .league.name // "" | ascii_downcase) | contains("lck")) |
            select((.match.teams[0].code // "" | ascii_downcase) == ("'"$team1"'" | ascii_downcase) or
                         (.match.teams[1].code // "" | ascii_downcase) == ("'"$team1"'" | ascii_downcase) or
                         (.match.teams[0].code // "" | ascii_downcase) == ("'"$team2"'" | ascii_downcase) or
                         (.match.teams[1].code // "" | ascii_downcase) == ("'"$team2"'" | ascii_downcase)) |
            .match | 
            "\(.teams[0].result.kills // .teams[0].result.gameWins // 0),\(.teams[1].result.kills // .teams[1].result.gameWins // 0)"
        ' 2>/dev/null | head -1
}

# Create menu using rofi or dmenu
show_menu() {
    local matches="$1"
    
    local match_count=$(echo "$matches" | grep -c . || echo 0)
    
    if [[ $match_count -eq 0 ]]; then
        notify-send "LCK" "No live matches currently" -u low
        exit 1
    fi
    
    if [[ $match_count -eq 1 ]]; then
        # Only one match, no need for selection
        local match_id=$(echo "$matches" | cut -d'|' -f1)
        echo "$match_id" > "$SELECTION_FILE"
        notify-send "LCK" "Already showing the only live match" -u low
        exit 0
    fi
    
    # Multiple matches - create menu
    local menu_options=""
    local declare -A match_map
    
    local line_num=0
    while IFS='|' read -r match_id team1 team2; do
        # Try to get kill counts
        local kills=$(get_match_kills "$team1" "$team2" || echo "")
        
        if [[ -n "$kills" ]]; then
            local team1_kills=$(echo "$kills" | cut -d',' -f1)
            local team2_kills=$(echo "$kills" | cut -d',' -f2)
            menu_options+="$team1 [$team1_kills] vs [$team2_kills] $team2\n"
        else
            menu_options+="$team1 vs $team2\n"
        fi
        
        match_map[$line_num]="$match_id"
        
        ((line_num++))
    done <<< "$matches"
    
    # Try rofi first, fallback to dmenu
    local selected=""
    
    if command -v rofi &> /dev/null; then
        selected=$(echo -e "$menu_options" | rofi -dmenu -p "Select Match:" -lines $match_count)
    elif command -v dmenu &> /dev/null; then
        selected=$(echo -e "$menu_options" | dmenu -p "Select Match:")
    else
        # Fallback: just show notification with all matches
        notify-send "LCK" "Available matches:\n$menu_options" -u normal
        exit 1
    fi
    
    if [[ -n "$selected" ]]; then
        # Find which match was selected
        local selected_num=$(echo -e "$menu_options" | grep -n "^$selected$" | cut -d':' -f1 | head -1)
        if [[ -n "$selected_num" ]]; then
            selected_num=$((selected_num - 1))
            if [[ -n "${match_map[$selected_num]:-}" ]]; then
                echo "${match_map[$selected_num]}" > "$SELECTION_FILE"
                notify-send "LCK" "Now showing: $selected" -u low
            fi
        fi
    fi
    
    # Refresh waybar
    pkill -RTMIN+1 waybar 2>/dev/null || true
}

# Main
matches=$(get_live_matches)

if [[ -z "$matches" ]]; then
    notify-send "LCK" "No live LCK matches" -u low
    exit 1
fi

show_menu "$matches"
