#!/usr/bin/env bash
# Live feed for LCK: shows kills and turrets when available
set -euo pipefail

CACHE_DIR="${XDG_CACHE_HOME:-$HOME/.cache}/lck-scores"
LIVE_CACHE="$CACHE_DIR/lck-live-games.json"
SCHEDULE_CACHE="$CACHE_DIR/lck-data.json"
SELECTION_FILE="$CACHE_DIR/selected-match.txt"
mkdir -p "$CACHE_DIR"

# API
API_BASE="https://esports-api.lolesports.com/persisted/gw"
LIVE_ENDPOINT="$API_BASE/getLive?hl=en-US"
SCHEDULE_ENDPOINT="$API_BASE/getSchedule?hl=en-US"

CURL_OPTS=( -s -m 10 -H "User-Agent: Mozilla/5.0" -H "Accept: application/json" -H "Referer: https://www.lolesports.com" -H "Origin: https://www.lolesports.com" )
API_KEY="${LCK_API_KEY:-0TvQnueqKa5mxJntVWt0w4LpLfEkrV1Ta8rQBb9Z}"
if [[ -n "$API_KEY" ]]; then
    CURL_OPTS+=( -H "Authorization: Bearer $API_KEY" -H "x-api-key: $API_KEY" )
fi

fetch_live() {
    if [[ -f "$LIVE_CACHE" ]]; then
        local file_age=$(( $(date +%s) - $(stat -c%Y "$LIVE_CACHE" 2>/dev/null || echo $(date +%s)) ))
        if [[ $file_age -lt 20 ]]; then
            cat "$LIVE_CACHE"
            return 0
        fi
    fi
    local resp
    resp=$(curl "${CURL_OPTS[@]}" "$LIVE_ENDPOINT" 2>/dev/null || echo "{}")
    printf '%s' "$resp" > "$LIVE_CACHE"
    printf '%s' "$resp"
}

fetch_schedule() {
    if [[ -f "$SCHEDULE_CACHE" ]]; then
        local file_age=$(( $(date +%s) - $(stat -c%Y "$SCHEDULE_CACHE" 2>/dev/null || echo $(date +%s)) ))
        if [[ $file_age -lt 60 ]]; then
            cat "$SCHEDULE_CACHE"
            return 0
        fi
    fi
    local resp
    resp=$(curl "${CURL_OPTS[@]}" "$SCHEDULE_ENDPOINT" 2>/dev/null || echo "{}")
    printf '%s' "$resp" > "$SCHEDULE_CACHE"
    printf '%s' "$resp"
}

live_data=$(fetch_live)
schedule_data=$(fetch_schedule)

python3 - "$LIVE_CACHE" "$SCHEDULE_CACHE" "$SELECTION_FILE" <<'PY'
import json,sys
live_file=sys.argv[1]
sched_file=sys.argv[2]
sel_file=sys.argv[3]
try:
    selected_id=''
    with open(sel_file) as f:
        selected_id=f.read().strip()
except Exception:
    selected_id=''

try:
    live=json.load(open(live_file))
except Exception:
    live={}

try:
    sched=json.load(open(sched_file))
except Exception:
    sched={}

def get_events(l):
    return (l.get('data',{}).get('esports',{}).get('events') or
            l.get('data',{}).get('schedule',{}).get('events') or [])

from datetime import datetime,timezone

def parse_iso(tstr):
    if not tstr:
        return None
    try:
        return datetime.strptime(tstr, "%Y-%m-%dT%H:%M:%SZ").replace(tzinfo=timezone.utc)
    except Exception:
        try:
            return datetime.fromisoformat(tstr)
        except Exception:
            return None

def next_start(events):
    now = datetime.now(timezone.utc)
    candidates = []
    for e in events:
        st = e.get('startTime') or e.get('beginAt') or e.get('start')
        dt = parse_iso(st)
        if dt and dt > now:
            candidates.append((dt,e))
    if not candidates:
        return None
    candidates.sort(key=lambda x: x[0])
    dt,e = candidates[0]
    return int((dt - now).total_seconds()), e

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

def find_key(obj, key):
    if obj is None:
        return None
    if isinstance(obj, dict):
        if key in obj and obj[key] is not None:
            return obj[key]
        for v in obj.values():
            r = find_key(v, key)
            if r is not None:
                return r
    elif isinstance(obj, list):
        for it in obj:
            r = find_key(it, key)
            if r is not None:
                return r
    return None

def get_field(team, *keys):
    def find_key(obj, key):
        if obj is None:
            return None
        if isinstance(obj, dict):
            if key in obj and obj[key] is not None:
                return obj[key]
            for v in obj.values():
                r = find_key(v, key)
                if r is not None:
                    return r
        elif isinstance(obj, list):
            for it in obj:
                r = find_key(it, key)
                if r is not None:
                    return r
        return None

    for k in keys:
        val = find_key(team, k)
        if val is not None:
            try:
                return int(val)
            except Exception:
                return val
    return 0

for e in get_events(live):
    region = ''
    tour = e.get('tournament') or {}
    if isinstance(tour, dict):
        region = tour.get('region') or ''
    league = e.get('league') or {}
    lname = ''
    if isinstance(league, dict):
        lname = (league.get('name') or league.get('slug') or '')
    status = (e.get('state') or e.get('status') or '').lower()
    mid = e.get('match',{}).get('id') or e.get('match_id') or e.get('id') or ''
    if 'lck' in (region or '').lower() or 'lck' in lname.lower():
        if selected_id and mid and mid != selected_id:
            continue
        if 'inprog' not in status:
            continue
        m = e.get('match') or {}
        teams = m.get('teams') or []
        if len(teams) < 2:
            continue
        t1=teams[0]
        t2=teams[1]
        name1 = t1.get('name') or t1.get('code') or ''
        name2 = t2.get('name') or t2.get('code') or ''
        # kills vs game wins: show game wins in brackets/summary, keep kills in tooltip
        k1 = get_field(t1, 'kills', 'killCount')
        k2 = get_field(t2, 'kills', 'killCount')
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
        tu1 = get_field(t1, 'turrets', 'towers', 'towerKills', 'turretKills')
        tu2 = get_field(t2, 'turrets', 'towers', 'towerKills', 'turretKills')

        # collect additional info for tooltip
        g1 = get_field(t1, 'gold', 'totalGold', 'goldEarned')
        g2 = get_field(t2, 'gold', 'totalGold', 'goldEarned')
        # game time
        time = e.get('gameTime') or m.get('gameTime') or e.get('game',{}).get('time') or ''
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

        def get_players(team):
            players = team.get('players') or team.get('roster') or team.get('participants') or []
            if isinstance(players, dict):
                players = list(players.values())
            out = []
            for p in players:
                if not isinstance(p, dict):
                    continue
                champ = p.get('championName') or p.get('champion') or p.get('selectedChampion') or p.get('pick') or ''
                name = p.get('summonerName') or p.get('name') or p.get('player') or ''
                if champ:
                    out.append(f"{name}({champ})" if name else champ)
            return out

        def normalize_name(team, fallback=''):
            if not team:
                return fallback or ''
            name = team.get('name') if isinstance(team, dict) else None
            if isinstance(name, str) and name.strip():
                return name
            if isinstance(name, list):
                for item in name:
                    if isinstance(item, str) and item.strip():
                        return item
                    if isinstance(item, dict):
                        n = item.get('name') or item.get('code')
                        if isinstance(n, str) and n.strip():
                            return n
            code = team.get('code') if isinstance(team, dict) else None
            if isinstance(code, str) and code.strip():
                return code
            if isinstance(team, list):
                for it in team:
                    if isinstance(it, str) and it.strip():
                        return it
                    if isinstance(it, dict):
                        n = it.get('name') or it.get('code')
                        if isinstance(n, str) and n.strip():
                            return n
            return fallback or ''

        p1 = get_players(t1)
        p2 = get_players(t2)

        # print main summary and tooltip details (append game length and next-match countdown if available)
        ft = format_time(time)
        # normalize names to avoid printing nested objects
        left = normalize_name(t1, name1)
        right = normalize_name(t2, name2)
        main = f"{left} [{gw1}] vs [{gw2}] {right}"
        if ft:
            main += f" ({ft})"
        # next upcoming match (from schedule data)
        ns = next_start(get_events(sched))
        if ns is not None:
            diff,next_event = ns
            lbl = format_delta(diff)
            main += f" ⏱️ {lbl}"
        print(main)

        print(f"Kills: {k1} - {k2} | Gold: {g1} - {g2} | Turrets: {tu1} - {tu2}")
        if p1:
            print(f"{name1} picks: {', '.join(p1)}")
        if p2:
            print(f"{name2} picks: {', '.join(p2)}")
        sys.exit(0)

print("No live LCK")
PY
