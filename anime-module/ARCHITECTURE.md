# Anime Module Architecture

## Runtime Flow

Waybar
-> `anime-updates.sh`
-> fetch MAL watching list (cached)
-> `anime_module.py`
-> Waybar JSON output

## Components

- `anime-updates.sh`
  - Loads `.env`
  - Builds MAL auth headers
  - Fetches and caches animelist JSON
  - Executes Python renderer

- `anime_module.py`
  - Loads cached MAL response
  - Computes recently released episodes (last 7 days)
  - Filters to unwatched episodes only
  - Builds Waybar text + multiline tooltip

- `waybar-anime-module.jsonc`
  - Example `custom/anime` config block

- `waybar-anime-style.css`
  - Visible style for updates
  - Hidden style when no updates

## Cache Files

Default cache path:
- `${XDG_CACHE_HOME:-$HOME/.cache}/anime-module`

Files:
- `mal-watching.json` (raw MAL API response)

## Hide Behavior

When no new unwatched episodes are found, module returns:

`{"text":"","tooltip":"","class":"hidden"}`

Waybar CSS class `hidden` removes spacing and visible text.
