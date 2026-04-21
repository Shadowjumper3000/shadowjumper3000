#!/usr/bin/env python3
"""
GitHub PR Selector - Right-click toggle logic
"""

import json
import os
from urllib.parse import urlparse


class GitHubPrSelector:
    """Cycle through tracked PRs and persist selection."""

    def __init__(self):
        self.cache_dir = os.path.expanduser(
            os.environ.get("CACHE_DIR", "~/.cache/github-pr-tracker")
        )
        self.involved_cache = os.path.join(self.cache_dir, "github-pr-involved.json")
        self.selection_file = os.path.join(self.cache_dir, "selected-pr.txt")

    @staticmethod
    def _load_json(filepath):
        """Safely load JSON file."""
        try:
            with open(filepath, encoding="utf-8") as f:
                return json.load(f)
        except Exception:
            return {}

    @staticmethod
    def _repo_from_url(repository_url):
        if not repository_url:
            return ""
        try:
            path = urlparse(repository_url).path
            parts = [p for p in path.split("/") if p]
            if len(parts) >= 3 and parts[0] == "repos":
                return f"{parts[1]}/{parts[2]}"
        except Exception:
            return ""
        return ""

    def get_open_prs(self):
        """Extract tracked PRs from involved cache."""
        data = self._load_json(self.involved_cache)
        items = data.get("items", [])
        if not isinstance(items, list):
            return []

        prs = []
        for item in items:
            if not isinstance(item, dict):
                continue

            pr_id = str(item.get("id", ""))
            if not pr_id:
                continue

            repo_full = self._repo_from_url(item.get("repository_url", ""))
            repo_short = repo_full.split("/")[-1] if repo_full else "repo"
            number = item.get("number", 0)
            title = item.get("title", "Untitled PR")

            prs.append(
                {
                    "id": pr_id,
                    "display": f"{repo_short}#{number}: {title}",
                }
            )

        return prs

    def toggle_to_next(self):
        """Move selection to next PR in list."""
        prs = self.get_open_prs()
        if not prs:
            return False

        current_idx = 0
        if os.path.exists(self.selection_file):
            try:
                with open(self.selection_file, encoding="utf-8") as f:
                    selected_id = f.read().strip()
                for i, pr in enumerate(prs):
                    if pr["id"] == selected_id:
                        current_idx = i
                        break
            except Exception:
                pass

        next_idx = (current_idx + 1) % len(prs)
        next_pr_id = prs[next_idx]["id"]

        try:
            with open(self.selection_file, "w", encoding="utf-8") as f:
                f.write(next_pr_id)
            print(f"Switched to: {prs[next_idx]['display']}")
            return True
        except Exception:
            return False


def main():
    selector = GitHubPrSelector()
    selector.toggle_to_next()


if __name__ == "__main__":
    main()
