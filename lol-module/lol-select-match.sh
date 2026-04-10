#!/usr/bin/env bash
# LoL Esports Match Selector - Right-click handler
# Toggles between multiple live matches across all LoL Esports regions

set -euo pipefail

LOG_FILE="/tmp/lol-selector-$(date +%s).log"

{
    echo "=== Handler started at $(date) ==="
    echo "PWD: $PWD"
    echo "USER: $USER"
    echo "Home: $HOME"
    echo "PATH: $PATH"
    
    # Get script directory
    SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    echo "Script dir: $SCRIPT_DIR"
    
    # Load environment configuration
    if [[ -f "$SCRIPT_DIR/.env" ]]; then
        echo "Loading .env from $SCRIPT_DIR/.env"
        set -a
        source "$SCRIPT_DIR/.env"
        set +a
    else
        echo "No .env file found at $SCRIPT_DIR/.env"
    fi
    
    # Set defaults if not in .env
    CACHE_DIR="${CACHE_DIR:-${XDG_CACHE_HOME:-$HOME/.cache}/lol-scores}"
    echo "Cache dir: $CACHE_DIR"
    
    # Verify cache directory exists
    if [[ ! -d "$CACHE_DIR" ]]; then
        echo "Cache dir doesn't exist, creating it"
        mkdir -p "$CACHE_DIR"
    fi
    
    # Check live cache
    LIVE_CACHE="$CACHE_DIR/lol-live-games.json"
    echo "Live cache: $LIVE_CACHE"
    if [[ -f "$LIVE_CACHE" ]]; then
        echo "Live cache exists, size: $(stat -c%s "$LIVE_CACHE" 2>/dev/null || stat -f%z "$LIVE_CACHE")"
    else
        echo "Live cache does NOT exist"
    fi
    
    # Check selection file
    SELECTION_FILE="$CACHE_DIR/selected-match.txt"
    echo "Selection file: $SELECTION_FILE"
    if [[ -f "$SELECTION_FILE" ]]; then
        echo "Current selection: $(cat "$SELECTION_FILE")"
    else
        echo "No current selection"
    fi
    
    echo ""
    echo "=== Running selector ==="
    
    # Run match selector Python module to toggle to next match
    export CACHE_DIR="$CACHE_DIR"
    python3 "$SCRIPT_DIR/lol_selector.py" 2>&1
    
    echo ""
    echo "=== After selector ==="
    if [[ -f "$SELECTION_FILE" ]]; then
        echo "New selection: $(cat "$SELECTION_FILE")"
    fi
    
    echo ""
    echo "=== Refreshing waybar ==="
    
    # Refresh waybar - try multiple methods
    pkill -RTMIN+1 waybar 2>&1 || echo "pkill RTMIN+1 failed"
    sleep 0.2
    pkill -HUP -f waybar 2>&1 || echo "pkill HUP failed"
    
    echo "=== Handler completed at $(date) ==="
    
} > "$LOG_FILE" 2>&1

echo "Logs written to: $LOG_FILE"
