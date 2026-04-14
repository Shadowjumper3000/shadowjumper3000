#!/usr/bin/env python3
"""Render Waybar JSON for MAL new-unwatched episodes in last 7 days."""

import json
import os
from datetime import datetime, time, timedelta, timezone


class AnimeModule:
    def __init__(self):
        self.cache_dir = os.path.expanduser(
            os.environ.get("CACHE_DIR", "~/.cache/anime-module")
        )
        self.animelist_cache = os.environ.get(
            "ANIMELIST_CACHE", os.path.join(self.cache_dir, "mal-watching.json")
        )
        self.max_title_len = self._as_int(os.environ.get("MAX_TITLE_LEN"), 28)
        self.week_window = timedelta(days=7)

    @staticmethod
    def _as_int(value, default=0):
        try:
            return int(value)
        except (TypeError, ValueError):
            return default

    @staticmethod
    def _load_json(path):
        try:
            with open(path, "r", encoding="utf-8") as fh:
                return json.load(fh)
        except (OSError, json.JSONDecodeError):
            return {}

    @staticmethod
    def _parse_date(value):
        if not value:
            return None

        value = str(value).strip()
        formats = ("%Y-%m-%d", "%Y-%m", "%Y")
        for fmt in formats:
            try:
                dt = datetime.strptime(value, fmt)
                return dt.replace(tzinfo=timezone.utc)
            except ValueError:
                continue
        return None

    @staticmethod
    def _parse_clock(value):
        if not value:
            return time(hour=0, minute=0)
        try:
            return datetime.strptime(value, "%H:%M").time()
        except ValueError:
            return time(hour=0, minute=0)

    @staticmethod
    def _weekday_index(day_name):
        mapping = {
            "monday": 0,
            "tuesday": 1,
            "wednesday": 2,
            "thursday": 3,
            "friday": 4,
            "saturday": 5,
            "sunday": 6,
        }
        if not day_name:
            return None
        return mapping.get(str(day_name).strip().lower())

    def _first_release(self, start_dt, broadcast):
        if not start_dt:
            return None

        release_time = self._parse_clock((broadcast or {}).get("start_time"))
        weekday = self._weekday_index((broadcast or {}).get("day_of_the_week"))

        release_date = start_dt.date()
        if weekday is not None:
            delta_days = (weekday - release_date.weekday()) % 7
            release_date = release_date + timedelta(days=delta_days)

        return datetime.combine(release_date, release_time, tzinfo=timezone.utc)

    @staticmethod
    def _episodes_aired_at(moment, first_release, total_episodes):
        if not first_release or moment < first_release:
            return 0

        weeks = int((moment - first_release).total_seconds() // (7 * 24 * 3600))
        aired = weeks + 1

        if total_episodes and total_episodes > 0:
            aired = min(aired, total_episodes)

        return max(aired, 0)

    @staticmethod
    def _watched_count(list_status):
        if not isinstance(list_status, dict):
            return 0

        for key in ("num_episodes_watched", "num_watched_episodes"):
            value = list_status.get(key)
            if isinstance(value, int):
                return max(value, 0)

        return 0

    def _trim_title(self, title):
        if len(title) <= self.max_title_len:
            return title
        if self.max_title_len <= 3:
            return title[: self.max_title_len]
        return title[: self.max_title_len - 3] + "..."

    def collect_updates(self):
        raw = self._load_json(self.animelist_cache)
        items = raw.get("data", [])
        now = datetime.now(timezone.utc)
        week_ago = now - self.week_window

        updates = []

        for item in items:
            if not isinstance(item, dict):
                continue

            node = item.get("node", {})
            list_status = item.get("list_status", {})

            title = str(node.get("title", "")).strip()
            if not title:
                continue

            watched = self._watched_count(list_status)
            total_episodes = self._as_int(node.get("num_episodes"), 0)
            start_dt = self._parse_date(node.get("start_date"))
            first_release = self._first_release(start_dt, node.get("broadcast"))

            released_now = self._episodes_aired_at(now, first_release, total_episodes)
            released_week_ago = self._episodes_aired_at(
                week_ago, first_release, total_episodes
            )

            if released_now <= released_week_ago:
                continue

            if watched >= released_now:
                continue

            first_unwatched = max(watched + 1, released_week_ago + 1)
            if first_unwatched > released_now:
                continue

            new_count = released_now - first_unwatched + 1
            updates.append(
                {
                    "title": title,
                    "title_short": self._trim_title(title),
                    "first_unwatched": first_unwatched,
                    "last_unwatched": released_now,
                    "new_count": new_count,
                }
            )

        updates.sort(key=lambda x: (-x["new_count"], x["title"].lower()))
        return updates

    @staticmethod
    def _episode_range(first_ep, last_ep):
        if first_ep == last_ep:
            return f"E{first_ep}"
        return f"E{first_ep}-{last_ep}"

    def render(self):
        updates = self.collect_updates()

        if not updates:
            return {"text": "", "tooltip": "", "class": "hidden"}

        total_new = sum(row["new_count"] for row in updates)
        first = updates[0]
        first_range = self._episode_range(
            first["first_unwatched"], first["last_unwatched"]
        )

        if len(updates) == 1:
            text = f"Anime +{total_new} {first['title_short']} {first_range}"
        else:
            text = f"Anime +{total_new} {first['title_short']} {first_range} (+{len(updates)-1})"

        tooltip_lines = [f"New unwatched episodes in last 7d: {total_new}"]
        for row in updates:
            ep_range = self._episode_range(
                row["first_unwatched"], row["last_unwatched"]
            )
            tooltip_lines.append(f"- {row['title']}: {ep_range} ({row['new_count']})")

        return {
            "text": text,
            "tooltip": "\n".join(tooltip_lines),
            "class": "has-updates",
        }


def main():
    module = AnimeModule()
    print(json.dumps(module.render(), ensure_ascii=False))


if __name__ == "__main__":
    main()
