#!/usr/bin/env bash
# LCK League of Legends Scores Module with detailed tooltip
set -euo pipefail

# Configuration
CACHE_DIR="${XDG_CACHE_HOME:-$HOME/.cache}/lck-scores"
CACHE_FILE="$CACHE_DIR/lck-data.json"
LIVE_CACHE="$CACHE_DIR/lck-live-games.json"
SELECTION_FILE="$CACHE_DIR/selected-match.txt"
CACHE_TTL=60

API_BASE="https://esports-api.lolesports.com/persisted/gw"
SCHEDULE_ENDPOINT="$API_BASE/getSchedule?hl=en-US"
LIVE_ENDPOINT="$API_BASE/getLive?hl=en-US"

CURL_OPTS=( -s -m 10 -H "User-Agent: Mozilla/5.0" -H "Accept: application/json" -H "Referer: https://www.lolesports.com" -H "Origin: https://www.lolesports.com" )
API_KEY="${LCK_API_KEY:-0TvQnueqKa5mxJntVWt0w4LpLfEkrV1Ta8rQBb9Z}"
if [[ -n "$API_KEY" ]]; then
    CURL_OPTS+=( -H "Authorization: Bearer $API_KEY" -H "x-api-key: $API_KEY" )
fi

mkdir -p "$CACHE_DIR"

PAD_LEFT="${LCK_PAD_LEFT:-  }"
PAD_RIGHT="${LCK_PAD_RIGHT:-  }"

get_cached_data() {
    if [[ -f "$CACHE_FILE" ]]; then
        local file_age=$(( $(date +%s) - $(stat -c%Y "$CACHE_FILE" 2>/dev/null || echo $(date +%s)) ))
        if [[ $file_age -lt $CACHE_TTL ]]; then
            cat "$CACHE_FILE"
            return 0
        fi
    fi
    return 1
}

fetch_lck_schedule() {
    if get_cached_data; then
        return 0
    fi
    local resp
    resp=$(curl "${CURL_OPTS[@]}" "$SCHEDULE_ENDPOINT" 2>/dev/null || echo "{}")
    printf '%s' "$resp" > "$CACHE_FILE"
    printf '%s' "$resp"
}

fetch_live_games() {
    if [[ -f "$LIVE_CACHE" ]]; then
        local file_age=$(( $(date +%s) - $(stat -c%Y "$LIVE_CACHE" 2>/dev/null || echo $(date +%s)) ))
        if [[ $file_age -lt 30 ]]; then
            cat "$LIVE_CACHE"
            return 0
        fi
    fi
    local resp
    resp=$(curl "${CURL_OPTS[@]}" "$LIVE_ENDPOINT" 2>/dev/null || echo "{}")
    printf '%s' "$resp" > "$LIVE_CACHE"
    printf '%s' "$resp"
}

format_waybar_output() {
    local json_data="$1"
    local live_data="$2"

    local tmp_schedule tmp_live
    tmp_schedule=$(mktemp)
    tmp_live=$(mktemp)
    printf '%s' "$json_data" > "$tmp_schedule"
    printf '%s' "$live_data" > "$tmp_live"

    python3 - "$tmp_schedule" "$tmp_live" "$PAD_LEFT" "$PAD_RIGHT" <<'PY'
import json,sys

sched = json.load(open(sys.argv[1])) if sys.argv[1] else {}
live = json.load(open(sys.argv[2])) if sys.argv[2] else {}
pad_left = sys.argv[3]
pad_right = sys.argv[4]

def events_from(obj):
    return (obj.get('data',{}).get('esports',{}).get('events') or
            obj.get('data',{}).get('schedule',{}).get('events') or [])

from datetime import datetime,timezone

def parse_iso(tstr):
    if not tstr:
        return None
    try:
        # expected format like 2026-04-09T08:00:00Z
        return datetime.strptime(tstr, "%Y-%m-%dT%H:%M:%SZ").replace(tzinfo=timezone.utc)
    except Exception:
        try:
            return datetime.fromisoformat(tstr)
        except Exception:
            return None

def next_start(schedule):
    now = datetime.now(timezone.utc)
    candidates = []
    for e in events_from(schedule):
        st = e.get('startTime') or e.get('beginAt') or e.get('start')
        dt = parse_iso(st)
        if dt and dt > now:
            candidates.append(dt)
    if not candidates:
        return None
    nxt = min(candidates)
    diff = int((nxt - now).total_seconds())
    return diff, nxt

def format_delta(seconds):
    if seconds is None:
        return ''
    s = int(seconds)
    if s <= 0:
        return 'now'
    m, sec = divmod(s, 60)
    h, m = divmod(m, 60)
    if h > 0:
        return f"in {h}h{m}m"
    if m > 0:
        return f"in {m}m"
    return f"in {sec}s"

def find_first_live(schedule):
    for e in events_from(schedule):
        league = e.get('league') or {}
        lname = (league.get('name') or league.get('slug') or '') if isinstance(league, dict) else str(league or '')
        status = (e.get('state') or e.get('status') or '').lower()
        if 'lck' in lname.lower() and 'inprog' in status:
            mid = e.get('match',{}).get('id') or e.get('match_id') or e.get('id') or ''
            t1 = (e.get('match',{}).get('teams') or [{}])[0]
            t2 = (e.get('match',{}).get('teams') or [{}])[1]
            return mid, (t1.get('name') or t1.get('code') or ''), (t2.get('name') or t2.get('code') or '')
    return '', '', ''

def find_key(obj, key):
    if obj is None:
        return None
    if isinstance(obj, dict):
        if key in obj and obj[key] is not None:
            return obj[key]
        for v in obj.values():
            res = find_key(v, key)
            if res is not None:
                return res
    elif isinstance(obj, list):
        for item in obj:
            res = find_key(item, key)
            if res is not None:
                return res
    return None

def get_field(team, *keys):
    # Search nested structures for the first matching key and return int if possible
    if team is None:
        return 0
    for k in keys:
        val = find_key(team, k)
        if val is not None:
            try:
                return int(val)
            except Exception:
                return val
    return 0

def get_picks(team):
    out = []
    players = team.get('players') or team.get('roster') or team.get('participants') or []
    if isinstance(players, dict):
        players = list(players.values())
    for p in players:
        if not isinstance(p, dict):
            continue
        champ = p.get('championName') or p.get('champion') or p.get('selectedChampion') or p.get('pick') or ''
        name = p.get('summonerName') or p.get('name') or p.get('player') or ''
        if champ:
            out.append(f"{name}({champ})" if name else champ)
    return out

def normalize_name(team, fallback=''):
    # team may contain name as string, list, or nested structures. Return a clean string.
    if not team:
        return fallback or ''
    # direct string
    name = team.get('name') if isinstance(team, dict) else None
    if isinstance(name, str) and name.strip():
        return name
    # if name is a list, try to extract string elements
    if isinstance(name, list):
        for item in name:
            if isinstance(item, str) and item.strip():
                return item
            if isinstance(item, dict):
                n = item.get('name') or item.get('code')
                if isinstance(n, str) and n.strip():
                    return n
    # code field
    code = team.get('code') if isinstance(team, dict) else None
    if isinstance(code, str) and code.strip():
        return code
    # sometimes team is a list itself
    if isinstance(team, list):
        for it in team:
            if isinstance(it, str) and it.strip():
                return it
            if isinstance(it, dict):
                n = it.get('name') or it.get('code')
                if isinstance(n, str) and n.strip():
                    return n
    return fallback or ''

mid, t1_name, t2_name = find_first_live(sched)
if mid:
    ev = None
    for e in events_from(live):
        m = e.get('match') or {}
        mid_e = m.get('id') or e.get('match_id') or e.get('id') or ''
        league = e.get('league') or {}
        lname = (league.get('name') or league.get('slug') or '') if isinstance(league, dict) else str(league or '')
        status = (e.get('state') or e.get('status') or '').lower()
        if mid_e == mid or ('lck' in lname.lower() and 'inprog' in status):
            ev = e
            break

    if ev is None:
        for e in events_from(sched):
            m = e.get('match') or {}
            mid_e = m.get('id') or e.get('match_id') or e.get('id') or ''
            if mid_e == mid:
                ev = e
                break

    summary = f"{pad_left}{t1_name} vs {t2_name}{pad_right}"

    if ev is not None:
        m = ev.get('match') or {}
        teams = m.get('teams') or []
        if len(teams) >= 2:
            t1 = teams[0]
            t2 = teams[1]
            # kills vs game wins: show game wins in bracketed summary, keep kills in tooltip
            kills1 = get_field(t1, 'kills', 'killCount') or 0
            kills2 = get_field(t2, 'kills', 'killCount') or 0
            gw1_raw = find_key(t1, 'gameWins')
            gw2_raw = find_key(t2, 'gameWins')
            try:
                gw1 = int(gw1_raw) if gw1_raw is not None else 0
            except Exception:
                gw1 = 0
            try:
                gw2 = int(gw2_raw) if gw2_raw is not None else 0
            except Exception:
                gw2 = 0
            g1 = get_field(t1, 'gold', 'totalGold', 'goldEarned') or 0
            g2 = get_field(t2, 'gold', 'totalGold', 'goldEarned') or 0
            tu1 = get_field(t1, 'turrets', 'towerKills', 'turretKills', 'towers') or 0
            tu2 = get_field(t2, 'turrets', 'towerKills', 'turretKills', 'towers') or 0
            time = ev.get('gameTime') or m.get('gameTime') or ev.get('game',{}).get('time') or ''
            def format_time(t):
                if not t:
                    return ''
                try:
                    ts = int(t)
                    m = ts // 60
                    s = ts % 60
                    return f"{m}:{s:02d}"
                except Exception:
                    tstr = str(t)
                    if ':' in tstr:
                        return tstr
                    return tstr
            p1 = get_picks(t1)
            p2 = get_picks(t2)
            d1 = get_field(t1, 'dragons', 'dragonKills')
            d2 = get_field(t2, 'dragons', 'dragonKills')
            b1 = get_field(t1, 'barons', 'baronKills')
            b2 = get_field(t2, 'barons', 'baronKills')

            ft = format_time(time)
            left = normalize_name(t1, t1_name)
            right = normalize_name(t2, t2_name)
            summary = f"{pad_left}{left} [{gw1}] vs [{gw2}] {right}"
            if ft:
                summary += f" ({ft})"
            summary += pad_right
            print(summary)
            print(f"Kills: {kills1} - {kills2} | Gold: {g1} - {g2} | Turrets: {tu1} - {tu2}")
            if (d1 or d2 or b1 or b2):
                print(f"Dragons: {d1} - {d2} | Barons: {b1} - {b2}")
            if time:
                print(f"Time: {time}")
            if p1:
                print(f"{t1.get('name') or t1.get('code') or t1_name} picks: {', '.join(p1)}")
            if p2:
                print(f"{t2.get('name') or t2.get('code') or t2_name} picks: {', '.join(p2)}")
            sys.exit(0)

    print(summary)
else:
    # show next upcoming LCK match with countdown
    ns = next_start(sched)
    if ns is not None:
        diff,nxt = ns
        label = format_delta(diff)
        # find the next upcoming event to show teams
        for e in events_from(sched):
            league = e.get('league') or {}
            lname = (league.get('name') or league.get('slug') or '') if isinstance(league, dict) else str(league or '')
            status = (e.get('state') or e.get('status') or '').lower()
            st = e.get('startTime') or e.get('beginAt') or e.get('start')
            dt = parse_iso(st)
            if 'lck' in lname.lower() and dt and dt > datetime.now(timezone.utc):
                t1 = (e.get('match',{}).get('teams') or [{}])[0]
                t2 = (e.get('match',{}).get('teams') or [{}])[1]
                left = normalize_name(t1)
                right = normalize_name(t2)
                print(f"{pad_left}⏱️ {left} vs {right} {label}{pad_right}")
                break
    else:
        # fallback generic upcoming
        for e in events_from(sched):
            league = e.get('league') or {}
            lname = (league.get('name') or league.get('slug') or '') if isinstance(league, dict) else str(league or '')
            status = (e.get('state') or e.get('status') or '').lower()
            if 'lck' in lname.lower() and 'not' in status:
                t1 = (e.get('match',{}).get('teams') or [{}])[0]
                t2 = (e.get('match',{}).get('teams') or [{}])[1]
                print(f"{pad_left}⏱️ {t1.get('name') or t1.get('code') or ''} vs {t2.get('name') or t2.get('code') or ''}{pad_right}")
                break

PY

    rm -f "$tmp_schedule" "$tmp_live"
}

main() {
    local action="${1:-}"
    if [[ "$action" == "refresh" ]]; then
        rm -f "$CACHE_FILE" "$LIVE_CACHE"
    fi

    local json_data
    json_data=$(fetch_lck_schedule)
    if [[ -z "$json_data" || "$json_data" == "{}" ]]; then
        echo "LCK"
        exit 1
    fi

    local live_data
    live_data=$(fetch_live_games)

    format_waybar_output "$json_data" "$live_data"
}

main "$@"
