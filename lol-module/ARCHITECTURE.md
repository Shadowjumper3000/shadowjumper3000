# LoL Module Architecture - Quick Reference

## Module Interaction Diagram

```
User/Waybar
    │
    ├── Left-click: open watch.lolesports.com
    │
    ├── Main Display (every 30s)
    │   └─> lol-scores.sh
    │       ├─ Load .env configuration
    │       ├─ Fetch & cache APIs (curl)
    │       └─ Call lol_module.py
    │           ├─ Load cached JSON
    │           ├─ Parse match data
    │           └─ Output JSON display
    │
    └── Right-click: cycle matches
        └─> lol-select-match.sh
            ├─ Load .env configuration
            └─ Call lol_selector.py
                ├─ Load cached JSON
                ├─ Get current selection
                ├─ Toggle to next match
                ├─ Save selection
                └─ Refresh Waybar
```

---

## Data Flow

### Display Flow (lol-scores.sh → lol_module.py)

```
lol-scores.sh                          lol_module.py
┌──────────────────────────────────┐  ┌──────────────────────────────────┐
│ 1. Load .env                     │  │ 3. Read cache files              │
│ 2. Fetch APIs (curl)             │  │    - lol-live-games.json        │
│    ↓                             │  │    - lol-data.json              │
│ 3. Cache to disk                 │  │ 4. Parse & extract data         │
│    - lol-live-games.json        │  │    - Team codes & names          │
│    - lol-data.json              │  │    - Series scores               │
│ 4. Call Python module            │→ │    - Season records              │
│                                  │  │ 5. Format output                 │
│                                  │  │    - Live: "T1 [1] vs [1] GEN" │
│                                  │  │    - Upcoming: "⏱️ in 2h"       │
│                                  │  │ 6. Output JSON                   │
│                                  │↓ │    - text, tooltip, class       │
│ Print to Waybar                  │← │                                  │
└──────────────────────────────────┘  └──────────────────────────────────┘
```

### Selection Flow (lol-select-match.sh → lol_selector.py)

```
lol-select-match.sh                    lol_selector.py
┌──────────────────────────────────┐  ┌──────────────────────────────────┐
│ 1. Load .env                     │  │ 2. Read cached live data         │
│ 2. Call Python module            │→ │    - lol-live-games.json        │
│                                  │  │ 3. Extract all matches          │
│ 3. Refresh Waybar                │  │ 4. Get current selection         │
│    pkill -RTMIN+1 waybar        │← │    from selected-match.txt      │
│                                  │  │ 5. Calculate next index         │
│                                  │  │ 6. Save new selection           │
│                                  │  │ 7. Print message                │
└──────────────────────────────────┘  └──────────────────────────────────┘
```

---

## Configuration Management

### .env File Structure

```
.env (1 per installation)
├─ API Endpoints
│  ├─ API_BASE
│  ├─ LIVE_ENDPOINT
│  ├─ SCHEDULE_ENDPOINT
│
├─ Authentication
│  └─ API_KEY
│
├─ Cache Settings
│  ├─ CACHE_DIR
│  ├─ LIVE_CACHE_TTL
│  └─ SCHEDULE_CACHE_TTL
│
└─ Display Settings
    ├─ SHOW_UPCOMING_HOURS
    └─ CURL_TIMEOUT
```

### Configuration Precedence

```
1. Environment variables (highest priority)
   API_KEY=my-key bash lol-scores.sh
   
2. .env file (if exists)
   source .env
   
3. Hardcoded defaults in scripts (lowest priority)
   API_KEY="${API_KEY:-default-key}"
```

---

## File Structure

### Python Modules

#### lol__module.py (Class: LolEsportsModule)
- **Purpose**: Display logic & data parsing
- **Input**: JSON cache files
- **Output**: JSON with text, tooltip, class
- **Methods**:
  - `get_live_matches()` → list[dict]
  - `output_live_match()` → dict|None
  - `output_upcoming_match()` → dict|None
  - `display()` → prints JSON

#### lol__selector.py (Class: LolEsportsMatchSelector)
- **Purpose**: Match selection logic
- **Input**: JSON cache, selected-match.txt
- **Output**: Persisted selection, console message
- **Methods**:
  - `get_live_matches()` → list[dict]
  - `toggle_to_next()` → bool

### Bash Scripts

#### lol-scores.sh
- Loads .env
- Fetches APIs with curl
- Caches JSON responses
- Calls `python3 lol__module.py`
- Output: JSON to Waybar

#### lol-select-match.sh
- Loads .env
- Calls `python3 lol__selector.py`
- Refreshes Waybar
- Output: Notification, selection saved

---

## Cache Structure

```
~/.cache/lol-scores/
├── lol-live-games.json        ← Current live data (30s TTL)
│   └─ API response from getLive endpoint
│
├── lol-data.json              ← Schedule data (60s TTL)
│   └─ API response from getSchedule endpoint
│
└── selected-match.txt         ← User's match selection
    └─ Contains match ID of selected match
```

---

## Error Handling

### Graceful Degradation

```
lol-scores.sh
├─ API unavailable?
│  └─ Uses cached data from previous run
│
├─ Cache missing?
│  └─ Empty cache files created, module returns empty
│
└─ Python module error?
   └─ No output sent to Waybar (module hidden)

lol-select-match.sh
├─ No live matches?
│  └─ "No live matches" message printed
│
└─ Only one match?
   └─ Cycles to itself, shows "Only match"
```

---

## Extension Points

### Add New Display Format
```python
# In lol__module.py

def output_custom_format(self):
    """Add custom output format"""
    matches = self.get_live_matches()
    return {
        "text": "CUSTOM FORMAT HERE",
        "tooltip": "...",
        "class": "custom-class"
    }

# In lol-scores.sh, add fallback

if [[ $DISPLAY_FORMAT == "custom" ]]; then
    python3 -c "from lol__module import LolEsportsModule; m=LolEsportsModule(); m.output_custom_format()"
fi
```

### Add New API Endpoint
```bash
# In .env
CUSTOM_API_ENDPOINT="https://..."
CUSTOM_API_CACHE_TTL=120

# In lol-scores.sh
fetch_api "$CUSTOM_API_ENDPOINT" "$CACHE_DIR/custom.json" "$CUSTOM_API_CACHE_TTL"
```

### Add New Selection Strategy
```python
# In lol__selector.py

def select_by_team(self, team_name):
    """Select match by team name"""
    matches = self.get_live_matches()
    for m in matches:
        if team_name.lower() in m['display'].lower():
            # Save this match
```

---

## Debugging

### Check Configuration
```bash
cd lol-module && source .env && env | grep -E '(API|CACHE|NOTIFY|SHOW)'
```

### Check Cache
```bash
cat ~/.cache/lol-scores/lol-live-games.json | jq .
cat ~/.cache/lol-scores/selected-match.txt
```

### Test Python Module Directly
```bash
cd lol-module && python3 lol__module.py | jq .
python3 lol__selector.py
```

### Check Bash Script Output
```bash
cd lol-module && bash lol-scores.sh
```

### View Waybar Logs
```bash
journalctl -u waybar -f
```

---

## Performance

### Cache TTLs (Configurable)
- Live games: 30s (frequently changing)
- Schedule: 60s (rarely changes during day)
- Selection: Persistent (until user changes)

### API Calls
- Fetch in parallel (background jobs in bash)
- Reused from cache when fresh
- Fallback to old cache if fetch fails

### Refresh Cycle
- Waybar interval: 30s
- Module output: Always fresh or cached
- Display update: Instantaneous when interval hits

---

## Security

### API Key Management
```bash
# .env file NOT committed to git
.gitignore:
    .env
    !.env.example

# Key rotation: just edit .env
API_KEY="new-key"
```

### Cache Files
```bash
# Located in user home cache dir
~/.cache/lol-scores/

# No sensitive data in cache
# Just structured JSON responses
```

---

## Backward Compatibility

This refactoring maintains **100% backward compatibility**:
- Same Waybar config (no changes needed)
- Same output format (text, tooltip, class)
- Same click behaviors (left/right)
- Same display interval (30s)

**Migration**: Just replace scripts, no config changes needed!
