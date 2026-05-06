#!/usr/bin/env python3
"""
LoL Esports Module - Core Display Logic
Handles fetching and processing match data from the LoL Esports API for all regions/leagues
"""

import hashlib
import json
import os
import subprocess
import time
from datetime import datetime, timezone


class LolModule:
    """Main LoL Esports module for displaying match information across all leagues"""

    def __init__(self, config_dir=None):
        self.config_dir = config_dir or os.path.dirname(os.path.abspath(__file__))
        self.cache_dir = os.path.expanduser(
            os.environ.get("CACHE_DIR", "~/.cache/lol-scores")
        )
        self.live_cache = os.path.join(self.cache_dir, "lol-live-games.json")
        self.schedule_cache = os.path.join(self.cache_dir, "lol-data.json")
        self.state_cache = os.path.join(self.cache_dir, "match-state.json")
        self.notification_dedupe_dir = os.path.join(
            self.cache_dir, "notification-dedupe"
        )
        self.notification_dedupe_ttl = int(
            os.environ.get("NOTIFICATION_DEDUPE_TTL", 21600)
        )

        os.makedirs(self.cache_dir, exist_ok=True)
        os.makedirs(self.notification_dedupe_dir, exist_ok=True)

        self.live_data = self._load_json(self.live_cache)
        self.sched_data = self._load_json(self.schedule_cache)
        self.previous_state = self._load_json(self.state_cache)
        self._cleanup_notification_keys()

        # Load excluded leagues from configuration file (JSON list) or env var
        # EXCLUDED_LEAGUES_FILE can point to a JSON file with an array of league
        # names. If not provided, fall back to the bundled excluded_leagues.json.
        self.excluded_leagues = self._load_excluded_leagues()

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

    @staticmethod
    def center_text(text, width=60):
        """Center align text by adding spaces"""
        lines = text.split("\n")
        centered_lines = []
        for line in lines:
            padding = max(0, (width - len(line)) // 2)
            centered_lines.append(" " * padding + line)
        return "\n".join(centered_lines)

    def get_current_game_number(self, match):
        """Get which game in the series is currently in progress"""
        if not isinstance(match, dict):
            return 0
        games = match.get("games", [])
        for game in games:
            if isinstance(game, dict) and game.get("state", "").lower() == "inprogress":
                return game.get("number", 0)
        return 0

    def get_game_elapsed_time(self, start_time_str):
        """Calculate elapsed time since match start (approx game length)"""
        try:
            start_dt = self.parse_iso(start_time_str)
            if not start_dt:
                return None
            now = datetime.now(timezone.utc)
            elapsed = (now - start_dt).total_seconds() / 60  # Convert to minutes
            if elapsed < 0:
                return None
            minutes = int(elapsed)
            seconds = int((elapsed % 1) * 60)
            return f"{minutes}:{seconds:02d}"
        except:
            return None

    def _cleanup_notification_keys(self):
        """Remove stale dedupe files so cache doesn't grow forever"""
        try:
            now = time.time()
            for name in os.listdir(self.notification_dedupe_dir):
                path = os.path.join(self.notification_dedupe_dir, name)
                try:
                    if now - os.path.getmtime(path) > self.notification_dedupe_ttl:
                        os.remove(path)
                except Exception:
                    continue
        except Exception:
            pass

    def _load_excluded_leagues(self):
        """Load excluded leagues from JSON file.

        The path can be overridden with the EXCLUDED_LEAGUES_FILE environment
        variable. If the file cannot be read or is invalid, return an empty set.
        Values are normalized to lowercase for comparison.
        """
        # Determine default path next to this file
        default_path = os.path.join(self.config_dir, "excluded_leagues.json")
        path = os.environ.get("EXCLUDED_LEAGUES_FILE", default_path)

        try:
            with open(path) as f:
                data = json.load(f)
            if isinstance(data, list):
                return {str(x).strip().lower() for x in data if x}
        except Exception:
            # Fail silently and return empty set
            return set()
        return set()

    def _claim_notification_key(self, dedupe_key):
        """Atomically claim dedupe key; return True only for first claimant"""
        if not dedupe_key:
            return True

        key_hash = hashlib.sha1(dedupe_key.encode("utf-8")).hexdigest()
        key_path = os.path.join(self.notification_dedupe_dir, key_hash)

        try:
            fd = os.open(key_path, os.O_CREAT | os.O_EXCL | os.O_WRONLY)
            with os.fdopen(fd, "w") as f:
                f.write(str(int(time.time())))
            return True
        except FileExistsError:
            return False
        except Exception:
            # Fail open to avoid losing notifications on filesystem errors.
            return True

    def send_notification(self, title, body, dedupe_key=None):
        """Send desktop notification using notify-send"""
        if not self._claim_notification_key(dedupe_key):
            return

        try:
            subprocess.run(
                ["notify-send", "-u", "normal", title, body], check=False, timeout=2
            )
        except Exception:
            pass

    def save_state(self, state):
        """Save current match state for comparison"""
        try:
            with open(self.state_cache, "w") as f:
                json.dump(state, f)
        except Exception:
            pass

    def check_for_victories(self, live_matches):
        """Check if any matches or games have been won"""
        if not os.environ.get("ENABLE_NOTIFICATIONS", "true").lower() == "true":
            return

        for match in live_matches:
            match_id = match.get("id", "")
            if not match_id:
                continue

            prev_match = self.previous_state.get(match_id, {})
            prev_gw1 = prev_match.get("gw1", 0)
            prev_gw2 = prev_match.get("gw2", 0)
            curr_gw1 = match.get("gw1", 0)
            curr_gw2 = match.get("gw2", 0)

            # Detect match victories first (series won - typically best of 3, so 2 wins)
            match_victory = False
            if os.environ.get("NOTIFY_MATCH_VICTORIES", "true").lower() == "true":
                if curr_gw1 >= 2 and prev_gw1 < 2:
                    self.send_notification(
                        f"🏆 {match['t1_name']} WINS THE MATCH!",
                        f"{match['t1_name']} defeats {match['t2_name']}\nFinal: {curr_gw1}-{curr_gw2}",
                        dedupe_key=f"match-victory:{match_id}:t1:{curr_gw1}-{curr_gw2}",
                    )
                    match_victory = True
                if curr_gw2 >= 2 and prev_gw2 < 2:
                    self.send_notification(
                        f"🏆 {match['t2_name']} WINS THE MATCH!",
                        f"{match['t2_name']} defeats {match['t1_name']}\nFinal: {curr_gw1}-{curr_gw2}",
                        dedupe_key=f"match-victory:{match_id}:t2:{curr_gw1}-{curr_gw2}",
                    )
                    match_victory = True

            # Detect game wins only if no match victory just happened
            if (
                not match_victory
                and os.environ.get("NOTIFY_GAME_WINS", "true").lower() == "true"
            ):
                if curr_gw1 > prev_gw1:
                    # Include the game number in the title, but keep the body small
                    # (remove the explicit "wins Game X" line to shorten notifications)
                    self.send_notification(
                        f"{match['t1_code']} wins Game {match['game_num']}!",
                        f"Series: {curr_gw1}-{curr_gw2}",
                        dedupe_key=f"game-win:{match_id}:t1:{curr_gw1}-{curr_gw2}",
                    )
                if curr_gw2 > prev_gw2:
                    # Include the game number in the title, but keep the body small
                    # (remove the explicit "wins Game X" line to shorten notifications)
                    self.send_notification(
                        f"{match['t2_code']} wins Game {match['game_num']}!",
                        f"Series: {curr_gw1}-{curr_gw2}",
                        dedupe_key=f"game-win:{match_id}:t2:{curr_gw1}-{curr_gw2}",
                    )

    def get_live_matches(self):
        """Find all currently live matches across all regions"""
        now = datetime.now(timezone.utc)
        live_matches = []

        for event in self.get_events(self.live_data):
            league = event.get("league", {})
            lname = league.get("name", "") if isinstance(league, dict) else ""
            # Skip excluded leagues
            if lname and self._is_excluded_league(lname):
                continue
            status = event.get("state", "").lower()

            # Skip if not in progress
            if "inprog" not in status:
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
            start_time = event.get("startTime", "")
            block_name = event.get("blockName", "")

            live_matches.append(
                {
                    "id": match.get("id", ""),
                    "league": lname,
                    "block_name": block_name,
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
                }
            )

        return live_matches

    def get_next_upcoming_match(self):
        """Find next upcoming match within configured hours"""
        now = datetime.now(timezone.utc)
        max_hours = int(os.environ.get("SHOW_UPCOMING_HOURS", 24))
        max_seconds = max_hours * 3600

        upcoming_matches = []

        for event in self.get_events(self.sched_data):
            league = event.get("league", {})
            lname = league.get("name", "") if isinstance(league, dict) else ""

            # Skip excluded leagues
            if lname and self._is_excluded_league(lname):
                continue

            dt = self.parse_iso(event.get("startTime"))
            state = event.get("state", "").lower()

            if not dt or "unstart" not in state:
                continue

            time_until = (dt - now).total_seconds()
            if time_until > max_seconds or time_until <= 0:
                continue

            upcoming_matches.append((dt, event, lname))

        if not upcoming_matches:
            return None, []

        # Sort by time and get first
        upcoming_matches.sort(key=lambda x: x[0])
        next_dt, next_event, next_league = upcoming_matches[0]

        return (next_dt, next_event, next_league), upcoming_matches

    def _is_excluded_league(self, league_name):
        """Return True if the league should be filtered out.

        This does a case-insensitive check against a small set of names and
        also handles minor variations by checking for key substrings (to
        tolerate accents/word differences).
        """
        if not league_name:
            return False
        n = league_name.strip().lower()

        # Direct match
        if n in self.excluded_leagues:
            return True

        # Keyword-based fuzzy matching for common variations
        if "rift" in n and "legend" in n:
            return True
        if "ligue" in n and ("franc" in n or "française" in n):
            return True
        if "arab" in n:
            return True

        return False

    # Selection feature has been removed; Waybar now shows count of live games

    def output_live_match(self):
        """Output live match information as JSON"""
        live_matches = self.get_live_matches()

        if not live_matches:
            return None

        # Show count of live games in the main display
        games_count = len(live_matches)
        text = f"{games_count} game{'s' if games_count != 1 else ''} live"

        # Group all matches by league for the tooltip
        by_league = {}
        for match in live_matches:
            league = match.get("league", "")
            if league not in by_league:
                by_league[league] = []
            by_league[league].append(match)

        # Build tooltip with league categories (keep previous detailed formatting)
        tooltip_lines = []
        for league in sorted(by_league.keys()):
            header = f"━ {league}"
            tooltip_lines.append(header)
            for match in by_league[league]:
                tooltip_lines.append(
                    f"  {match['t1_code']} [{match['gw1']}] vs [{match['gw2']}] {match['t2_code']}"
                )

        return {"text": text, "tooltip": "\n".join(tooltip_lines), "class": "lol-live"}

    def output_upcoming_match(self):
        """Output upcoming match information as JSON"""
        next_match_info, upcoming_matches = self.get_next_upcoming_match()

        if not next_match_info:
            return None

        next_dt, next_event, next_league = next_match_info
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
        block_name = next_event.get("blockName", "Upcoming")

        main = f"⏱️ {t1_code} vs {t2_code} (in {countdown})"
        tooltip_lines = [
            f"Upcoming: {t1_name} vs {t2_name}",
            f"{t1_code} ({rec1[0]}-{rec1[1]}) vs {t2_code} ({rec2[0]}-{rec2[1]})",
        ]

        # Show next few upcoming matches in tooltip
        if len(upcoming_matches) > 1:
            tooltip_lines.append("")
            tooltip_lines.append("Next matches:")
            for dt, evt, league in upcoming_matches[1:3]:
                m = evt.get("match", {})
                ts = evt.get("teams", [])
                if len(ts) >= 2:
                    time_until = self.format_countdown((dt - now).total_seconds())
                    tooltip_lines.append(
                        f"  {league}: {self.get_team_code(ts[0])} vs {self.get_team_code(ts[1])} (in {time_until})"
                    )

        return {
            "text": main,
            "tooltip": self.center_text(f"{block_name}\n" + "\n".join(tooltip_lines)),
            "class": "lol-upcoming",
        }

    def display(self):
        """Display module output"""
        # Check for match victories and game wins
        live_matches = self.get_live_matches()
        if live_matches:
            self.check_for_victories(live_matches)

            # Save current state for next comparison
            state_dict = {
                m["id"]: {"gw1": m["gw1"], "gw2": m["gw2"]} for m in live_matches
            }
            self.save_state(state_dict)

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
    module = LolModule()
    module.display()


if __name__ == "__main__":
    main()
