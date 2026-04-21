#!/usr/bin/env bash
# GitHub PR Tracker - Right-click handler
# Cycles selected PR and refreshes Waybar module

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [[ -f "$SCRIPT_DIR/.env" ]]; then
    set -a
    source "$SCRIPT_DIR/.env"
    set +a
fi

CACHE_DIR="${CACHE_DIR:-${XDG_CACHE_HOME:-$HOME/.cache}/github-pr-tracker}"
mkdir -p "$CACHE_DIR"

export CACHE_DIR="$CACHE_DIR"
python3 "$SCRIPT_DIR/github_pr_selector.py"

# Refresh waybar to render updated selection quickly.
pkill -RTMIN+1 waybar 2>/dev/null || true
pkill -HUP -f waybar 2>/dev/null || true
