# Changelog

## 1.0.0 - 2026-10-19

First release.

- Picture-in-picture video drawn inside the game's own frame (works in fullscreen and borderless, on Windows and
  under CrossOver on macOS). One SKSE plugin for Skyrim SE/AE 1.5.97, 1.6.x, and 1.7.x; tested on Enderal SE
  (Skyrim 1.7.104). No ESP, nothing stored in saves.
- Plays local files, direct video links, and HLS streams through libmpv (LGPL).
- YouTube links (videos and live streams) through the bundled or user-supplied yt-dlp and QuickJS, at up to 720p by
  default, preferring H.264. Switch quality while playing; **Update yt-dlp** button in the control panel.
- Control panel (`[`): pauses the game; URL box with paste, Play clipboard, Recent videos with Resume, pause, stop,
  volume, and live layout settings (corner, size, opacity, margins) saved to the INI.
- On-video controls while the panel is open: title, play/pause, 5 second skips, seek bar; click to pause, drag to
  move the video to another corner.
- Quick keys `]` (pause) and `\` (show/hide), remappable by name in the INI; the log warns about keys that are also
  game controls.
- Remembers the last 100 videos and where you left them; resumes the last video when a save loads.
- Shows what it is doing while a video starts (looking up, opening, buffering).
- Release archives: the plugin alone, the plugin with the YouTube tools, and the YouTube tools alone.
