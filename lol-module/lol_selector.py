#!/usr/bin/env python3
"""
LoL Esports Match Selector - Match toggling logic
Handles cycling through live matches across all leagues
"""

import json
import os
import sys


class LolMatchSelector:
    """Handle match selection and toggling"""

    def __init__(self):
        self.cache_dir = os.path.expanduser(
            os.environ.get("CACHE_DIR", "~/.cache/lol-scores")
        )
        self.live_cache = os.path.join(self.cache_dir, "lol-live-games.json")
        self.selection_file = os.path.join(self.cache_dir, "selected-match.txt")
        self.debug = True

    @staticmethod
    def _load_json(filepath):
        """Safely load JSON file"""
        try:
            with open(filepath) as f:
                return json.load(f)
        except Exception as e:
            print(f"Debug: Failed to load {filepath}: {e}", file=sys.stderr)
            return {}

    def get_live_matches(self):
        """Extract all live matches across all regions"""
        data = self._load_json(self.live_cache)

        if self.debug:
            print(f"Debug: Using cache_dir: {self.cache_dir}", file=sys.stderr)
            print(f"Debug: Live cache path: {self.live_cache}", file=sys.stderr)

        matches = []
        for event in data.get("data", {}).get("schedule", {}).get("events", []):
            league = event.get("league", {})
            league_name = league.get("name", "")

            state = event.get("state", "").lower()
            if "inprog" not in state:
                continue

            match = event.get("match", {})
            teams = match.get("teams", [])
            if len(teams) < 2:
                continue

            match_id = match.get("id", "")
            t1_code = teams[0].get("code", teams[0].get("name", "?"))
            t2_code = teams[1].get("code", teams[1].get("name", "?"))
            gw1 = teams[0].get("result", {}).get("gameWins", 0)
            gw2 = teams[1].get("result", {}).get("gameWins", 0)

            matches.append(
                {
                    "id": match_id,
                    "league": league_name,
                    "display": f"{league_name}: {t1_code} [{gw1}] vs [{gw2}] {t2_code}",
                }
            )

        if self.debug:
            print(f"Debug: Found {len(matches)} live matches", file=sys.stderr)
        return matches

    def toggle_to_next(self):
        """Toggle to the next live match"""
        matches = self.get_live_matches()

        if not matches:
            if self.debug:
                print("Debug: No live matches found", file=sys.stderr)
            return False

        # Get current selection
        current_idx = 0
        if os.path.exists(self.selection_file):
            try:
                with open(self.selection_file) as f:
                    selected_id = f.read().strip()
                if self.debug:
                    print(
                        f"Debug: Current selection file contains: {selected_id}",
                        file=sys.stderr,
                    )
                for i, m in enumerate(matches):
                    if m["id"] == selected_id:
                        current_idx = i
                        break
            except Exception as e:
                if self.debug:
                    print(f"Debug: Error reading selection file: {e}", file=sys.stderr)
        else:
            if self.debug:
                print(
                    f"Debug: Selection file doesn't exist: {self.selection_file}",
                    file=sys.stderr,
                )

        # Toggle to next match
        next_idx = (current_idx + 1) % len(matches)
        next_match_id = matches[next_idx]["id"]

        if self.debug:
            print(
                f"Debug: Toggling from index {current_idx} to {next_idx}",
                file=sys.stderr,
            )
            print(f"Debug: New match ID: {next_match_id}", file=sys.stderr)

        try:
            with open(self.selection_file, "w") as f:
                f.write(next_match_id)
            print(f"Switched to: {matches[next_idx]['display']}")
            if self.debug:
                print(f"Debug: Successfully wrote selection file", file=sys.stderr)
            return True
        except Exception as e:
            if self.debug:
                print(f"Debug: Error writing selection file: {e}", file=sys.stderr)
            return False


def main():
    """Main entry point"""
    selector = LolMatchSelector()
    selector.toggle_to_next()


if __name__ == "__main__":
    main()
