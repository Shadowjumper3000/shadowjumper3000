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

    @staticmethod
    def _load_json(filepath):
        """Safely load JSON file"""
        try:
            with open(filepath) as f:
                return json.load(f)
        except:
            return {}

    def get_live_matches(self):
        """Extract all live matches across all regions"""
        data = self._load_json(self.live_cache)

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

        return matches

    def toggle_to_next(self):
        """Toggle to the next live match"""
        matches = self.get_live_matches()

        if not matches:
            print("No live matches")
            return False

        # Get current selection
        current_idx = 0
        if os.path.exists(self.selection_file):
            try:
                with open(self.selection_file) as f:
                    selected_id = f.read().strip()
                for i, m in enumerate(matches):
                    if m["id"] == selected_id:
                        current_idx = i
                        break
            except:
                pass

        # Toggle to next match
        next_idx = (current_idx + 1) % len(matches)
        next_match_id = matches[next_idx]["id"]

        try:
            with open(self.selection_file, "w") as f:
                f.write(next_match_id)
            print(f"Switched to: {matches[next_idx]['display']}")
            return True
        except:
            return False


def main():
    """Main entry point"""
    selector = LolMatchSelector()
    selector.toggle_to_next()


if __name__ == "__main__":
    main()
