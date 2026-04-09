# LCK League of Legends Waybar Module

Waybar module to display League of Legends LCK (League Champions Korea) live game scores, kill counts, and upcoming matches.

## Features

- ✨ Shows live LCK games in real-time with 🔴 indicator
- 📊 **Displays kill counts during live matches** (format: `🔴 T1 [5] vs [3] GEN.G`)
- 🎯 **Multiple match support** - Shows count of additional games (+1, +2, etc.)
- 🔄 **Right-click match selection** - Choose which game to display when multiple are running
- ⏱️ Displays upcoming LCK matches  
- 📊 Pulls data from the official LoL Esports API
- ⚡ Caching for optimal performance (30-60s TTL)
- 🎨 Beautiful styling with live game animations
- 🔗 One-click access to live viewer page
- 📲 Tooltip with match information

## API Used

This module uses the **Unofficial LoL Esports API** (`esports-api.lolesports.com`) which provides:
- Live match schedules
- Team information
- Match status and scores
- Game state updates with kill counts

API Endpoints:
- `https://esports-api.lolesports.com/persisted/gw/getSchedule?hl=en-US` - Schedule data
- `https://esports-api.lolesports.com/persisted/gw/getLive?hl=en-US` - Live game data with kill counts

## Files Included

- **lck-scores.sh** - Main waybar module script with kill count support
- **lck-select-match.sh** - Match selection menu for right-click action
- **lck-scores-detailed.sh** - Detailed information display (useful for tooltips/popups)
- **waybar-lck-module.jsonc** - Waybar module configuration
- **waybar-lck-style.css** - CSS styling for the module
- **install-lck-module.sh** - Automated installation script
- **README.md** - Documentation (this file)

## Installation

### 1. Copy files to your system

```bash
# Copy the scripts
cp lck-module/lck-scores.sh ~/.local/bin/
cp lck-module/lck-select-match.sh ~/.local/bin/
cp lck-module/lck-scores-detailed.sh ~/.local/bin/

# Make them executable
chmod +x ~/.local/bin/lck-*.sh
```

Or use the automated installer:
```bash
cd lck-module
./install-lck-module.sh
```

Or reference them directly from the repo location:
```bash
# In your waybar config, use:
"exec": "$HOME/dev/shadowjumper3000/lck-module/lck-scores.sh"
```

### 2. Add module to Waybar config

Edit `~/.config/waybar/config.jsonc` and add the LCK module to your modules list:

```jsonc
{
  "modules-right": [
    // ... other modules ...
    "custom/lck",
    // ... more modules ...
  ],
  
  "custom/lck": {
    "format": "{}",
    "exec": "$HOME/dev/shadowjumper3000/lck-module/lck-scores.sh",
    "interval": 30,
    "return-type": "plain",
    "on-click": "$HOME/dev/shadowjumper3000/lck-module/lck-scores.sh refresh && pkill -RTMIN+1 waybar && xdg-open 'https://andydanger.github.io/live-lol-esports/'",
    "on-right-click": "$HOME/dev/shadowjumper3000/lck-module/lck-select-match.sh",
    "tooltip": true,
    "tooltip-format": "{}",
    "class": "lck-module"
  }
}
```

### 3. Add styling to Waybar CSS

Edit `~/.config/waybar/style.css` and append the contents of `waybar-lck-style.css`:

```bash
cat lck-module/waybar-lck-style.css >> ~/.config/waybar/style.css
```

Or copy the CSS rules manually from [waybar-lck-style.css](waybar-lck-style.css).

### 4. Install dependencies

The scripts require:
- `bash` (already installed)
- `curl` (for API requests)
- `jq` (for JSON parsing)
- `rofi` or `dmenu` (for match selection menu, optional but recommended)

```bash
# Arch Linux
sudo pacman -S curl jq rofi

# Ubuntu/Debian
sudo apt install curl jq rofi

# macOS
brew install curl jq rofi
```

If neither rofi nor dmenu is installed, match selection will show a notification with all available matches instead.

## Usage

### Basic Display

Once installed, the module will appear in your waybar showing:
- **LCK** - No games scheduled
- **🔴 T1 [5] vs [3] GEN.G** - Live match with kill counts
- **🔴 T1 [8] vs [4] DK +1** - Live match with additional games available
- **⏱️ T1 vs DK** - Upcoming match with timer indicator

### Interactions

- **Left Click** - Refreshes data and opens the live viewer
- **Right Click** - Shows menu to select which match to display (when multiple are live)

### Kill Count Display

When a match is live:
- Format: `🔴 TEAM1 [Kills] vs [Kills] TEAM2`
- Kills update every 30 seconds
- Shows even during the first few seconds of a match (when kills are still 0:0)

### Multiple Match Selection

When multiple LCK matches are running simultaneously:
- Main display shows the selected match with a counter: `+1`, `+2`, etc.
- Right-click to open a dropdown menu with all active matches
- Select a match to switch what's displayed
- Updated with live kill counts for quick comparison

Example menu:
```
T1 [12] vs [8] GEN.G
DK [5] vs [6] KT
```

### Manual Commands

```bash
# Show current status
./lck-module/lck-scores.sh

# Force refresh and clear cache
./lck-module/lck-scores.sh refresh

# Show detailed information
./lck-module/lck-scores-detailed.sh

# Open match selection menu manually
./lck-module/lck-select-match.sh
```

## Customization

### Change Update Interval

In your `~/.config/waybar/config.jsonc`, modify the `interval`:
```jsonc
"interval": 30,  // Update every 30 seconds (default: 30 for live kill count data)
```

Note: Live games update more frequently to show kill count changes. Consider using 15-30 seconds for live matches, 60+ seconds when no games are running.

### Change Cache TTL

Edit `lck-scores.sh` and modify these variables:
```bash
CACHE_TTL=60  # Cache for 60 seconds for schedule data
# In fetch_live_games(): 30  # Cache for 30 seconds for live data with kills
```

### Modify Styling

Edit `~/.config/waybar/style.css` to customize colors:

```css
#custom-lck.lck-live {
  background: linear-gradient(90deg, #ff6b6b 0%, #ff8800 100%);
}

#custom-lck.lck-upcoming {
  background-color: #4ecdc4;
}
```

Available CSS classes:
- `.lck-live` - Applied when there are live games
- `.lck-upcoming` - Applied when there are upcoming games  
- `.lck-offline` - Applied when no games are scheduled

## Performance Notes

- Live data caches for 30 seconds to minimize API calls
- Schedule data caches for 60 seconds
- Typical API response time: 100-300ms
- Cache location: `~/.cache/lck-scores/`
- Memory usage: < 10MB
- Kill count updates every 30 seconds during live games

## Troubleshooting

### Module not showing up
1. Verify the script path in `config.jsonc` is correct
2. Test the script manually: `$HOME/dev/shadowjumper3000/lck-module/lck-scores.sh`
3. Check waybar logs: `journalctl --user-unit waybar -f`

### No kill count data
1. Kill counts may not be available immediately at game start
2. Wait 2-3 minutes for first blood/kills
3. Check if the API endpoint is responding: `curl https://esports-api.lolesports.com/persisted/gw/getLive?hl=en-US | jq . | head -50`

### No data showing
1. Check internet connection: `curl -s https://esports-api.lolesports.com/persisted/gw/getSchedule | jq . | head -20`
2. Verify API is responsive
3. Clear cache: `rm -f ~/.cache/lck-scores/lck-*.json`

### Match selection menu not appearing
1. Install rofi: `sudo pacman -S rofi` (or appropriate package manager)
2. Fallback: A notification will show if rofi/dmenu not available
3. Test manually: `./lck-module/lck-select-match.sh`

### Performance issues
- Increase the `interval` value (currently 30 seconds)
- Increase `CACHE_TTL` in the scripts
- Reduce update frequency during off-hours

## Light/Dark Mode

The styling uses Nord theme colors but adapts to your waybar theme. For custom themes, adjust these color variables in your CSS.

## Privacy

- No data is stored on your system except cached API responses
- Cache is auto-purged after 30-60 seconds
- No tracking or analytics

## License

MIT License - Feel free to modify and share!

## Related Resources

- [Live LoL Esports Dashboard](https://andydanger.github.io/live-lol-esports/)
- [LoL Esports Official](https://lolesports.com/)
- [Waybar Documentation](https://github.com/Alexays/Waybar)
- [LCK Official Site](https://www.lck.kr/)
- [Rofi GitHub](https://github.com/davatorium/rofi)

## Support

For issues or suggestions:
1. Check the Troubleshooting section above
2. Verify dependencies are installed
3. Test with manual script execution
4. Check API endpoint directly with curl

Enjoy watching LCK! 🎮
