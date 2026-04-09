# LoL Module - Complete API Reference

## API Endpoints

### 1. Live Games Endpoint
**URL**: `https://esports-api.lolesports.com/persisted/gw/getLive?hl=en-US`

**Purpose**: Get currently live and recently completed matches across all LoL regions

**Cache TTL**: 30 seconds

**Response Format**: JSON

---

## Complete API Response Fields

### Live Games Response Structure

```
{
  "data": {
    "schedule": {
      "events": [
        {
          // Event-level fields
          "id": "string (unique event ID)",
          "startTime": "2026-04-09T08:00:00Z (ISO 8601 timestamp)",
          "state": "string (inProgress | completed | unstarted)",
          "type": "string (match)",
          "blockName": "string (Week 2 | Playoffs Round 1 | etc)",
          
          // League information
          "league": {
            "id": "string (98767991310872058 for LCK, varies by league)",
            "slug": "string (lck, lpl, lcl, etc)",
            "name": "string (LCK, LPL, LCL, etc)",
            "image": "string (league logo URL)",
            "priority": number (1 for LCK, varies by league),
            "displayPriority": {
              "position": number,
              "status": "string"
            }
          },
          
          // Tournament
          "tournament": {
            "id": "string (tournament ID)"
          },
          
          // Match details
          "match": {
            "id": "string (match ID)",
            
            "teams": [
              // Team 1 (Index 0)
              {
                "id": "string (unique team ID)",
                "name": "string (KIWOOM DRX)",
                "slug": "string (drx)",
                "code": "string (3-letter: KRX, BRO, T1, GEN, DK, FOX, LSB, KDF, etc)",
                "image": "string (team logo URL)",
                
                "result": {
                  "outcome": null | "string (win | loss)",
                  "gameWins": number (0-3: current series score)
                },
                
                "record": {
                  "wins": number (season wins),
                  "losses": number (season losses)
                }
              },
              // Team 2 (Index 1)
              {
                "id": "string",
                "name": "string (HANJIN BRION)",
                "slug": "string (fredit-brion)",
                "code": "string (BRO)",
                "image": "string (team logo URL)",
                
                "result": {
                  "outcome": null | "string",
                  "gameWins": number (1 in this example)
                },
                
                "record": {
                  "wins": number,
                  "losses": number
                }
              }
            ],
            
            "strategy": {
              "type": "string (bestOf)",
              "count": number (3 for Best of 3, 5 for Best of 5)
            },
            
            "games": [
              {
                "number": number (1-5),
                "id": "string (game ID)",
                "state": "string (completed | inProgress | unstarted)",
                "teams": [
                  {
                    "id": "string (team ID)",
                    "side": "string (blue | red)"
                  },
                  {
                    "id": "string",
                    "side": "string (blue | red)"
                  }
                ],
                "vods": []
              },
              // ... more games
            ]
          },
          
          // Broadcast streams
          "streams": [
            {
              "parameter": "string (lck, lpl, lcl, pcs, vcs, cblol, etc)",
              "locale": "string (en-US, ko-KR, fr-FR, etc)",
              "mediaLocale": {
                "locale": "string",
                "englishName": "string",
                "translatedName": "string"
              },
              "provider": "string (twitch, afreecatv, youtube)",
              "countries": ["string (country codes)"],
              "offset": number (milliseconds offset),
              "statsStatus": "string (enabled | disabled)"
            }
          ]
        }
      ]
    }
  }
}
```

---

### 2. Schedule Endpoint
**URL**: `https://esports-api.lolesports.com/persisted/gw/getSchedule?hl=en-US`

**Purpose**: Get full season schedule including upcoming and past matches across all regions

**Cache TTL**: 60 seconds

**Response Format**: JSON

```
{
  "data": {
    "schedule": {
      "events": [
        {
          // Event-level fields
          "startTime": "2026-04-09T08:00:00Z (ISO 8601 timestamp)",
          "state": "string (completed | unstarted)",
          "type": "string (match)",
          "blockName": "string (Week X, Playoffs, etc)",
          
          // League information
          "league": {
            "id": "string",
            "slug": "string (lck, lpl, lcl, etc)",
            "name": "string (LCK, LPL, LCL, etc)",
            "image": "string (URL)",
            "priority": number,
            "displayPriority": { ... }
          },
          
          // Match details
          "match": {
            "id": "string (match ID)",
            "flags": [],
            
            "teams": [
              {
                "name": "string (team name)",
                "code": "string (3-letter code)",
                "image": "string (team logo URL)",
                
                "result": {
                  "outcome": null | "string",
                  "gameWins": number
                },
                
                "record": {
                  "wins": number,
                  "losses": number
                }
              },
              // ... team 2
            ],
            
            "strategy": {
              "type": "string",
              "count": number
            }
          }
        }
      ]
    }
  }
}
```

---

## Available Data for Module Display

### ✅ Data Available from API

**When Live (state: "inProgress")**:
- Team names and codes → Display format: `TEAM1 vs TEAM2`
- Series wins (gameWins) → Display format: `[X] vs [Y]`
- Season record (wins/losses) → For tooltip
- Current game number and state → For tooltip info
- Match start time → Elapsed time calculation
- Block name → Context (Week X, Playoffs)

**When Upcoming (state: "unstarted")**:
- Team names and codes
- Scheduled start time → Calculate countdown (e.g., "in 2h 30m")
- Season record
- Block name

### ❌ Data NOT Available from API
- **Kill counts** - Game live stats not in this API
- **Gold amounts** - Would need separate game stats API
- **Turret/Tower kills** - Not included in this API
- **Baron/Dragon counts** - Not in API
- **Champion picks** - Not in API
- **Live game position data** - Would need spectator API

---

## Example Responses

### Example 1: Live Match Response
```json
{
  "state": "inProgress",
  "match": {
    "teams": [
      {
        "code": "KRX",
        "name": "KIWOOM DRX",
        "result": { "gameWins": 1 },
        "record": { "wins": 0, "losses": 2 }
      },
      {
        "code": "BRO",
        "name": "HANJIN BRION",
        "result": { "gameWins": 1 },
        "record": { "wins": 1, "losses": 1 }
      }
    ],
    "games": [
      { "number": 1, "state": "completed" },
      { "number": 2, "state": "completed" },
      { "number": 3, "state": "inProgress" }
    ]
  }
}

// Display Output:
// Main: "KRX [1] vs [1] BRO"
// Tooltip: "Game 3 of 3 | KRX (0-2) vs BRO (1-1)"
```

### Example 2: Upcoming Match Response
```json
{
  "state": "unstarted",
  "startTime": "2026-04-10T16:00:00Z",
  "match": {
    "teams": [
      {
        "code": "T1",
        "name": "T1",
        "record": { "wins": 4, "losses": 2 }
      },
      {
        "code": "GEN",
        "name": "Gen.G",
        "record": { "wins": 3, "losses": 2 }
      }
    ]
  }
}

// Display Output (if game live: hidden)
// When showing timer: "⏱️ T1 vs Gen.G in 2h 45m"
// Tooltip: "Upcoming: T1 (4-2) vs Gen.G (3-2)"
```

### Example 3: No Live Match
```json
// Events array contains only completed or unstarted matches
// Module should:
// - Show hidden if no match within next 24 hours
// - Show timer for next upcoming match if within display window
// - In tooltip, show next 3 upcoming matches
```

---

## Module Behavior Matrix

| Scenario | Display | Click | Hover Tooltip |
|----------|---------|-------|---------------|
| **Game Live (1 match)** | `TEAM1 [X] vs [Y] TEAM2` | Open stream | Detailed stats, season record, current game |
| **Game Live (2+ matches)** | +N badge | Choose match menu | Show which team selected |
| **No game, next in < 24h** | ⏱️ Timer | Show next | Schedule of upcoming |
| **No game, next > 24h** | **HIDDEN** | N/A | N/A |
| **Empty schedule** | **HIDDEN** | N/A | N/A |

---

## Use Cases Supported

1. **Real-time Display** - Shows live match scores and teams
2. **Match Selection** - Right-click menu to pick which game to display
3. **Upcoming Awareness** - Shows countdown to next match
4. **Information Preview** - Hover tooltip for detailed info
5. **Auto-hide** - Hidden when nothing relevant is happening
