#!/usr/bin/env python3
"""
LCK Module - Core Display Logic
Handles fetching and processing LCK match data from the LoL Esports API
"""

import json
import sys
import os
from datetime import datetime, timezone


class LCKModule:
    """Main LCK module for displaying match information"""

    def __init__(self, config_dir=None):
        self.config_dir = config_dir or os.path.dirname(os.path.abspath(__file__))
        self.cache_dir = os.path.expanduser(
            os.environ.get("LCK_CACHE_DIR", "~/.cache/lck-scores")
        )
        self.live_cache = os.path.join(self.cache_dir, "lck-live-games.json")
        self.schedule_cache = os.path.join(self.cache_dir, "lck-data.json")
        self.selection_file = os.path.join(self.cache_dir, "selected-match.txt")

        os.makedirs(self.cache_dir, exist_ok=True)

        self.live_data = self._load_json(self.live_cache)
        self.sched_data = self._load_json(self.schedule_cache)

    @staticmethod
    def _load_json(filepath):
        """Safely load JSON file"""
        try:
            with open(filepath) as f:
                return json.load(f)
        except:
            return {}

    def get_events(self, obj):
        """Extract events from API response"""
        return obj.get("data", {}).get("schedule", {}).get("events", [])

    @staticmethod
    def get_team_code(team):
        """Extract team code (3-letter code or name)"""
        if not isinstance(team, dict):
            return ""
        return team.get("code", "") or team.get("name", "")

    @staticmethod
    def get_team_name(team):
        """Extract full team name"""
        if not isinstance(team, dict):
            return ""
        return team.get("name", "") or team.get("code", "")

    @staticmethod
    def parse_iso(ts):
        """Parse ISO 8601 timestamp"""
        if not ts:
            return None
        try:
            return datetime.strptime(ts, "%Y-%m-%dT%H:%M:%SZ").replace(
                tzinfo=timezone.utc
            )
        except:
            return None

    @staticmethod
    def format_countdown(sec):
        """Format seconds as human-readable countdown"""
        if not sec or sec <= 0:
            return ""
        s = int(sec)
        m, sec = divmod(s, 60)
        h, m = divmod(m, 60)
        if h > 0:
            return f"{h}h{m}m"
        if m > 0:
            return f"{m}m"
        return f"{s}s"

    @staticmethod
    def get_game_wins(team):
        """Get series wins from team result"""
        if not isinstance(team, dict):
            return 0
        result = team.get("result", {})
        if isinstance(result, dict):
            return result.get("gameWins", 0)
        return 0

    @staticmethod
    def get_record(team):
        """Get season record (wins, losses) from team"""
        if not isinstance(team, dict):
            return (0, 0)
        record = team.get("record", {})
        if isinstance(record, dict):
            return (record.get("wins", 0), record.get("losses", 0))
        return (0, 0)

    def get_current_game_number(self, match):
        """Get which game in the series is currently in progress"""
        if not isinstance(match, dict):
            return 0
        games = match.get("games", [])
        for game in games:
            if isinstance(game, dict) and game.get("state", "").lower() == "inprogress":
                return game.get("number", 0)
        return 0

    def get_live_matches(self):
        """Find all currently live LCK matches"""
        now = datetime.now(timezone.utc)
        live_matches = []

        for event in self.get_events(self.live_data):
            league = event.get("league", {})
            lname = league.get("name", "").lower() if isinstance(league, dict) else ""
            status = event.get("state", "").lower()

            if "lck" not in lname or "inprog" not in status:
                continue

            match = event.get("match", {})
            teams = match.get("teams", [])
            if len(teams) < 2:
                continue

            t1, t2 = teams[0], teams[1]
            t1_code = self.get_team_code(t1)
            t2_code = self.get_team_code(t2)
            gw1 = self.get_game_wins(t1)
            gw2 = self.get_game_wins(t2)
            rec1 = self.get_record(t1)
            rec2 = self.get_record(t2)
            game_num = self.get_current_game_number(match)

            live_matches.append(
                {
                    "id": match.get("id", ""),
                    "t1_code": t1_code,
                    "t2_code": t2_code,
                    "t1_name": self.get_team_name(t1),
                    "t2_name": self.get_team_name(t2),
                    "gw1": gw1,
                    "gw2": gw2,
                    "rec1": rec1,
                    "rec2": rec2,
                    "game_num": game_num,
                    "display": f"{t1_code} [{gw1}] vs [{gw2}] {t2_code}",
                    "tooltip_title": f"Game {game_num} - {self.get_team_name(t1)} vs {self.get_team_name(t2)}",
                    "tooltip_info": f"{t1_code} ({rec1[0]}-{rec1[1]}) vs {t2_code} ({rec2[0]}-{rec2[1]})",
                }
            )

        return live_matches

    def get_next_upcoming_match(self):
        """Find next upcoming match within configured hours"""
        now = datetime.now(timezone.utc)
        max_hours = int(os.environ.get("LCK_SHOW_UPCOMING_HOURS", 24))
        max_seconds = max_hours * 3600

        upcoming_matches = []

        for event in self.get_events(self.sched_data):
            league = event.get("league", {})
            if "lck" not in league.get("name", "").lower():
                continue

            dt = self.parse_iso(event.get("startTime"))
            state = event.get("state", "").lower()

            if not dt or "unstart" not in state:
                continue

            time_until = (dt - now).total_seconds()
            if time_until > max_seconds or time_until <= 0:
                continue

            upcoming_matches.append((dt, event))

        if not upcoming_matches:
            return None, []

        # Sort by time and get first
        upcoming_matches.sort(key=lambda x: x[0])
        next_dt, next_event = upcoming_matches[0]

        return (next_dt, next_event), upcoming_matches

    def get_selected_match_index(self, live_matches):
        """Get the index of the selected match or default to 0"""
        selected_idx = 0
        if os.path.exists(self.selection_file):
            try:
                with open(self.selection_file) as f:
                    selected_id = f.read().strip()
                for i, m in enumerate(live_matches):
                    if m["id"] == selected_id:
                        selected_idx = i
                        break
            except:
                pass
        return selected_idx

    def output_live_match(self):
        """Output live match information as JSON"""
        live_matches = self.get_live_matches()

        if not live_matches:
            return None

        # Get selected match
        selected_idx = self.get_selected_match_index(live_matches)
        match = live_matches[selected_idx]

        # Build tooltip
        tooltip_lines = [match["tooltip_title"], match["tooltip_info"]]

        # Show if there are other matches
        if len(live_matches) > 1:
            other_count = len(live_matches) - 1
            tooltip_lines.append(
                f"\n+{other_count} other match{'es' if other_count > 1 else ''} live"
            )
            tooltip_lines.append("(Right-click to switch)")

        return {
            "text": match["display"],
            "tooltip": "\n".join(tooltip_lines),
            "class": "lck-live",
        }

    def output_upcoming_match(self):
        """Output upcoming match information as JSON"""
        next_match_info, upcoming_matches = self.get_next_upcoming_match()

        if not next_match_info:
            return None

        next_dt, next_event = next_match_info
        now = datetime.now(timezone.utc)

        match = next_event.get("match", {})
        teams = match.get("teams", [])
        if len(teams) < 2:
            return None

        t1_code = self.get_team_code(teams[0])
        t2_code = self.get_team_code(teams[1])
        t1_name = self.get_team_name(teams[0])
        t2_name = self.get_team_name(teams[1])
        countdown = self.format_countdown((next_dt - now).total_seconds())
        rec1 = self.get_record(teams[0])
        rec2 = self.get_record(teams[1])

        main = f"⏱️ {t1_code} vs {t2_code} (in {countdown})"
        tooltip_lines = [
            f"Upcoming: {t1_name} vs {t2_name}",
            f"{t1_code} ({rec1[0]}-{rec1[1]}) vs {t2_code} ({rec2[0]}-{rec2[1]})",
        ]

        # Show next few upcoming matches in tooltip
        if len(upcoming_matches) > 1:
            tooltip_lines.append("")
            tooltip_lines.append("Next matches:")
            for dt, evt in upcoming_matches[1:3]:
                m = evt.get("match", {})
                ts = evt.get("teams", [])
                if len(ts) >= 2:
                    time_until = self.format_countdown((dt - now).total_seconds())
                    tooltip_lines.append(
                        f"  {self.get_team_code(ts[0])} vs {self.get_team_code(ts[1])} (in {time_until})"
                    )

        return {
            "text": main,
            "tooltip": "\n".join(tooltip_lines),
            "class": "lck-upcoming",
        }

    def display(self):
        """Display module output"""
        # Try live match first
        live_output = self.output_live_match()
        if live_output:
            print(json.dumps(live_output))
            return

        # Try upcoming match
        upcoming_output = self.output_upcoming_match()
        if upcoming_output:
            print(json.dumps(upcoming_output))
            return

        # No match to display - module hidden (no output)


def main():
    """Main entry point"""
    module = LCKModule()
    module.display()


if __name__ == "__main__":
    main()
