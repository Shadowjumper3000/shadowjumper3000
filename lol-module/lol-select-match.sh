#!/usr/bin/env bash
# LoL Esports Match Selector - Right-click handler
# Toggles between multiple live matches across all LoL Esports regions

set -euo pipefail

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Load environment configuration
if [[ -f "$SCRIPT_DIR/.env" ]]; then
    set -a
    source "$SCRIPT_DIR/.env"
    set +a
fi

# Run match selector Python module
python3 "$SCRIPT_DIR/lol_selector.py"

# Refresh waybar to show updated match
pkill -RTMIN+1 waybar 2>/dev/null || true
    local selected=""
    
    if command -v rofi &> /dev/null; then
        selected=$(echo -e "$menu_options" | rofi -dmenu -p "Select Match:" -lines $match_count)
    elif command -v dmenu &> /dev/null; then
        selected=$(echo -e "$menu_options" | dmenu -p "Select Match:")
    else
        # Fallback: just show notification with all matches
        notify-send "LoL Esports" "Available matches:\n$menu_options" -u normal
        exit 1
    fi
    
    if [[ -n "$selected" ]]; then
        # Find which match was selected
        local selected_num=$(echo -e "$menu_options" | grep -n "^$selected$" | cut -d':' -f1 | head -1)
        if [[ -n "$selected_num" ]]; then
            selected_num=$((selected_num - 1))
            if [[ -n "${match_map[$selected_num]:-}" ]]; then
                echo "${match_map[$selected_num]}" > "$SELECTION_FILE"
                notify-send "LoL Esports" "Now showing: $selected" -u low
            fi
        fi
    fi
    
    # Refresh waybar
    pkill -RTMIN+1 waybar 2>/dev/null || true
}

# Main
matches=$(get_live_matches)

if [[ -z "$matches" ]]; then
    notify-send "LoL Esports" "No live matches" -u low
    exit 1
fi

show_menu "$matches"
