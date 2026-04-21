#!/usr/bin/env python3
"""
GitHub PR Tracker Module - Core display and notification logic
"""

import hashlib
import json
import os
import subprocess
import time
from datetime import datetime, timezone
from urllib.parse import urlparse


class GitHubPrModule:
    """Display tracked PRs and send deduped notifications for key changes."""

    def __init__(self, config_dir=None):
        self.config_dir = config_dir or os.path.dirname(os.path.abspath(__file__))
        self.cache_dir = os.path.expanduser(
            os.environ.get("CACHE_DIR", "~/.cache/github-pr-tracker")
        )
        self.involved_cache = os.path.join(self.cache_dir, "github-pr-involved.json")
        self.review_cache = os.path.join(self.cache_dir, "github-pr-review.json")
        self.selection_file = os.path.join(self.cache_dir, "selected-pr.txt")
        self.state_cache = os.path.join(self.cache_dir, "pr-state.json")
        self.notification_dedupe_dir = os.path.join(
            self.cache_dir, "notification-dedupe"
        )
        self.notification_dedupe_ttl = int(
            os.environ.get("NOTIFICATION_DEDUPE_TTL", 21600)
        )
        self.max_title_length = int(os.environ.get("MAX_TITLE_LENGTH", 70))

        os.makedirs(self.cache_dir, exist_ok=True)
        os.makedirs(self.notification_dedupe_dir, exist_ok=True)

        self.involved_data = self._load_json(self.involved_cache)
        self.review_data = self._load_json(self.review_cache)
        self.previous_state = self._load_json(self.state_cache)
        if not isinstance(self.previous_state, dict):
            self.previous_state = {}

        self._cleanup_notification_keys()

    @staticmethod
    def _load_json(filepath):
        """Safely load JSON file."""
        try:
            with open(filepath, encoding="utf-8") as f:
                return json.load(f)
        except Exception:
            return {}

    @staticmethod
    def _parse_iso(ts):
        """Parse GitHub timestamp as UTC datetime."""
        if not ts:
            return None
        try:
            return datetime.strptime(ts, "%Y-%m-%dT%H:%M:%SZ").replace(
                tzinfo=timezone.utc
            )
        except Exception:
            return None

    @staticmethod
    def _repo_from_url(repository_url):
        """Convert API repository URL into owner/repo name."""
        if not repository_url:
            return ""
        try:
            path = urlparse(repository_url).path
            if not path:
                return ""
            parts = [p for p in path.split("/") if p]
            # Expected format: /repos/{owner}/{repo}
            if len(parts) >= 3 and parts[0] == "repos":
                return f"{parts[1]}/{parts[2]}"
        except Exception:
            return ""
        return ""

    @staticmethod
    def _truncate(text, max_len):
        """Trim long text for compact waybar tooltips."""
        if not text:
            return ""
        if max_len <= 3:
            return text[:max_len]
        if len(text) <= max_len:
            return text
        return text[: max_len - 3] + "..."

    def _cleanup_notification_keys(self):
        """Remove stale dedupe files."""
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

    def _claim_notification_key(self, dedupe_key):
        """Atomically claim dedupe key; True for first claimant only."""
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
            # Fail open so temporary fs issues do not drop notifications.
            return True

    def send_notification(self, title, body, dedupe_key=None):
        """Send desktop notification using notify-send."""
        if not self._claim_notification_key(dedupe_key):
            return

        try:
            subprocess.run(
                ["notify-send", "-u", "normal", title, body], check=False, timeout=2
            )
        except Exception:
            pass

    def save_state(self, state):
        """Persist current PR state for next diff."""
        try:
            with open(self.state_cache, "w", encoding="utf-8") as f:
                json.dump(state, f)
        except Exception:
            pass

    def _bool_env(self, name, default="false"):
        return os.environ.get(name, default).lower() == "true"

    def get_open_prs(self):
        """Build normalized PR list from cached GitHub API search responses."""
        involved_items = self.involved_data.get("items", [])
        review_items = self.review_data.get("items", [])

        if not isinstance(involved_items, list):
            involved_items = []
        if not isinstance(review_items, list):
            review_items = []

        review_requested_ids = {
            str(item.get("id"))
            for item in review_items
            if isinstance(item, dict) and item.get("id")
        }

        prs = []
        for item in involved_items:
            if not isinstance(item, dict):
                continue

            pr_id = str(item.get("id", ""))
            if not pr_id:
                continue

            repo_full = self._repo_from_url(item.get("repository_url", ""))
            repo_short = repo_full.split("/")[-1] if repo_full else "repo"
            user = item.get("user") if isinstance(item.get("user"), dict) else {}
            labels = item.get("labels") if isinstance(item.get("labels"), list) else []

            prs.append(
                {
                    "id": pr_id,
                    "number": item.get("number", 0),
                    "title": item.get("title", "Untitled PR"),
                    "html_url": item.get("html_url", ""),
                    "updated_at": item.get("updated_at", ""),
                    "repo_full": repo_full,
                    "repo_short": repo_short,
                    "author": user.get("login", ""),
                    "labels": [
                        lbl.get("name", "")
                        for lbl in labels
                        if isinstance(lbl, dict) and lbl.get("name")
                    ],
                    "is_review_requested": pr_id in review_requested_ids,
                }
            )

        prs.sort(
            key=lambda pr: (
                self._parse_iso(pr.get("updated_at"))
                or datetime.min.replace(tzinfo=timezone.utc),
                str(pr.get("number", 0)),
            ),
            reverse=True,
        )

        return prs

    def get_selected_pr_index(self, prs):
        """Return selected PR index from persistent selection file."""
        selected_idx = 0
        if os.path.exists(self.selection_file):
            try:
                with open(self.selection_file, encoding="utf-8") as f:
                    selected_id = f.read().strip()
                for i, pr in enumerate(prs):
                    if pr["id"] == selected_id:
                        selected_idx = i
                        break
            except Exception:
                pass
        return selected_idx

    def check_for_updates(self, prs):
        """Compare cached state and send deduped notifications."""
        notifications_enabled = self._bool_env("ENABLE_NOTIFICATIONS", "true")

        current_state = {}
        for pr in prs:
            pr_id = pr["id"]
            current_state[pr_id] = {
                "updated_at": pr.get("updated_at", ""),
                "review_requested": bool(pr.get("is_review_requested", False)),
                "repo_full": pr.get("repo_full", ""),
                "number": pr.get("number", 0),
                "title": pr.get("title", ""),
            }

        if not notifications_enabled:
            self.save_state(current_state)
            return

        notify_new = self._bool_env("NOTIFY_NEW_PRS", "true")
        notify_review = self._bool_env("NOTIFY_REVIEW_REQUESTS", "true")
        notify_updates = self._bool_env("NOTIFY_PR_UPDATES", "false")
        notify_removed = self._bool_env("NOTIFY_PR_REMOVED", "true")

        for pr in prs:
            pr_id = pr["id"]
            prev = self.previous_state.get(pr_id, {})
            is_new = not prev

            if is_new and notify_new:
                self.send_notification(
                    f"New PR: {pr['repo_short']}#{pr['number']}",
                    self._truncate(pr["title"], 120),
                    dedupe_key=f"pr-new:{pr_id}:{pr.get('updated_at', '')}",
                )

            if (
                notify_review
                and bool(pr.get("is_review_requested", False))
                and (is_new or not bool(prev.get("review_requested", False)))
            ):
                self.send_notification(
                    f"Review requested: {pr['repo_short']}#{pr['number']}",
                    self._truncate(pr["title"], 120),
                    dedupe_key=f"pr-review:{pr_id}:{pr.get('updated_at', '')}",
                )

            if (
                notify_updates
                and (not is_new)
                and prev.get("updated_at", "") != pr.get("updated_at", "")
            ):
                self.send_notification(
                    f"PR updated: {pr['repo_short']}#{pr['number']}",
                    self._truncate(pr["title"], 120),
                    dedupe_key=f"pr-updated:{pr_id}:{pr.get('updated_at', '')}",
                )

        if notify_removed:
            for pr_id, prev in self.previous_state.items():
                if pr_id in current_state:
                    continue
                repo_full = prev.get("repo_full", "")
                repo_short = repo_full.split("/")[-1] if repo_full else "repo"
                number = prev.get("number", "?")
                self.send_notification(
                    f"PR resolved: {repo_short}#{number}",
                    self._truncate(prev.get("title", "No longer in tracked set"), 120),
                    dedupe_key=f"pr-removed:{pr_id}:{prev.get('updated_at', '')}",
                )

        self.save_state(current_state)

    def output_open_prs(self, prs):
        """Build Waybar JSON payload for tracked PR list."""
        if not prs:
            return None

        selected_idx = self.get_selected_pr_index(prs)
        selected = prs[selected_idx]

        by_repo = {}
        for pr in prs:
            repo = pr["repo_full"] or "unknown/repo"
            by_repo.setdefault(repo, []).append(pr)

        tooltip_lines = []
        for repo in sorted(by_repo.keys()):
            tooltip_lines.append(f"-- {repo}")
            for pr in by_repo[repo]:
                marker = "*" if pr["id"] == selected["id"] else " "
                review_marker = "R" if pr["is_review_requested"] else " "
                title = self._truncate(pr["title"], self.max_title_length)
                tooltip_lines.append(
                    f"{marker} [{review_marker}] #{pr['number']} {title}"
                )

        selected_prefix = "R " if selected["is_review_requested"] else ""
        text = f"PR {selected_prefix}{selected['repo_short']}#{selected['number']}"
        if len(prs) > 1:
            text = f"{text} +{len(prs) - 1}"

        tooltip_lines.append("")
        tooltip_lines.append("R = review requested")

        return {
            "text": text,
            "tooltip": "\n".join(tooltip_lines),
            "class": (
                "github-pr-review"
                if selected["is_review_requested"]
                else "github-pr-open"
            ),
        }

    def display(self):
        """Render module output and run notification checks."""
        prs = self.get_open_prs()
        self.check_for_updates(prs)

        output = self.output_open_prs(prs)
        if output:
            print(json.dumps(output))


def main():
    module = GitHubPrModule()
    module.display()


if __name__ == "__main__":
    main()
