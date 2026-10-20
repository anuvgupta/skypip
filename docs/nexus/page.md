# PiP Player - picture-in-picture videos in Skyrim

## About

PiP Player plays a video in a corner of the screen while you keep playing Skyrim or Enderal. Paste a YouTube link,
a direct video link, a livestream, or a file on your PC; it plays at the size, corner, and opacity you choose, and
picks up where you left it next time.

It is drawn inside the game's own frame, so it works in exclusive fullscreen and borderless window, on Windows and
under CrossOver on macOS.

## Features

- YouTube videos and live streams, local video files, direct links (`.mp4`, `.webm`, ...), and HLS streams
- Control panel that pauses the game: paste a link, play the clipboard, recent videos with Resume, pause, stop,
  volume, quality, brightness, and layout settings applied live
- On-video controls: title, play/pause, 5 second skips, seek bar; click the video to pause, drag it to another corner
- Switch YouTube quality while playing (360p to 4K, 720p by default)
- Remembers the last 100 videos and where you left them; resumes your last video when you load a save
- Quick keys for pause and show/hide; all keys remappable
- No ESP, no scripts, nothing stored in saves: add or remove it at any time

## Requirements

- [SKSE64](https://skse.silverlock.org/) for your game version
- [Address Library for SKSE Plugins](https://www.nexusmods.com/skyrimspecialedition/mods/32444) (Enderal SE already
  includes it)
- Skyrim Special Edition / Anniversary Edition 1.5.97, 1.6.x, or 1.7.x, or Enderal SE
- For YouTube: yt-dlp and QuickJS (included in the "with YouTube" download, or see below)

## Installation

Install with Mod Organizer 2 or Vortex, or extract the archive into your game's `Data` folder. There is no plugin to
enable in the load order.

### Downloads

| File | What it contains | What it plays |
|---|---|---|
| **PiPPlayer with YouTube** | plugin, video player (libmpv), yt-dlp, QuickJS | everything |
| **PiPPlayer** | plugin, video player (libmpv) | local files, direct links, streams (no YouTube) |
| **PiPPlayer YouTube Tools** | yt-dlp, QuickJS | adds YouTube to the plain PiPPlayer download |

All three are on the [GitHub releases page](https://github.com/anuvgupta/skypip/releases/latest).

### Getting YouTube support

If you installed the plain **PiPPlayer** file, pick one:

1. **Add the YouTube Tools download.** Get `PiPPlayer-YouTube-Tools-<version>.zip` from
   [GitHub releases](https://github.com/anuvgupta/skypip/releases/latest) and install it like any mod (or extract it
   into `Data`).
2. **Get the two tools yourself.**
   - `yt-dlp_win.zip` from [yt-dlp releases](https://github.com/yt-dlp/yt-dlp/releases/latest) (tested: 2026.08.19);
     extract it so `yt-dlp.exe` and its `_internal` folder end up in the PiPPlayer folder (the single-file
     `yt-dlp.exe` also works but starts slower)
   - `qjs-windows-x86_64.exe` from [QuickJS-NG releases](https://github.com/quickjs-ng/quickjs/releases/latest)
     (tested: v0.17.0); rename it to `qjs.exe`
   - The PiPPlayer folder is `Data/SKSE/Plugins/PiPPlayer/` (where `libmpv-2.dll` is). With MO2, use that folder
     inside the PiPPlayer mod (right-click the mod → Open in Explorer).
3. **Use the all-in-one download.** Replace PiPPlayer with `PiPPlayer-<version>-with-YouTube.zip` from
   [GitHub releases](https://github.com/anuvgupta/skypip/releases/latest).

Without the tools, YouTube links show a message pointing here; everything else plays normally.

### Files

```
Data/SKSE/Plugins/PiPPlayer.dll
Data/SKSE/Plugins/PiPPlayer.ini
Data/SKSE/Plugins/PiPPlayer/libmpv-2.dll
Data/SKSE/Plugins/PiPPlayer/yt-dlp.exe      (YouTube)
Data/SKSE/Plugins/PiPPlayer/_internal/      (YouTube, yt-dlp's files)
Data/SKSE/Plugins/PiPPlayer/qjs.exe         (YouTube)
```

## How to use

1. Load a save and press `[` to open the control panel. The game pauses and a mouse cursor appears.
2. Paste a link (Ctrl+V) into the box and press **Play**, or copy a link first and press **Play clipboard**, or pick
   a video from **Recent** and press **Resume**.
3. Press `[`, Esc, or the panel's X to close it. The video keeps playing in its corner while you play.

While the panel is open, the video shows its title, a play/pause button, 5 second skip buttons, and a seek bar.
Click the video to pause it, or drag it to move it to another corner.

### Controls

| Key | Action |
|---|---|
| `[` | Open or close the control panel |
| `]` | Pause or resume |
| `\` | Show or hide the player |
| Esc | Close the control panel |

These are the three keys right of P on a US keyboard; on other layouts the same physical keys are used. On a Mac
under CrossOver, Cmd+V also pastes in the panel when CrossOver passes Cmd through.

## Configuration

Most settings are in the control panel and are saved automatically. Everything else is in
`Data/SKSE/Plugins/PiPPlayer.ini` (edit it while the game is closed).

| Setting | Default | |
|---|---|---|
| `[Overlay] Corner` | `BottomLeft` | `TopLeft`, `TopRight`, `BottomLeft`, `BottomRight` |
| `Width` | `480` | video width in pixels at the game's resolution |
| `MarginX`, `MarginY` | `24` | distance from the screen edges |
| `Opacity` | `1.0` | `0.05` to `1.0` |
| `Brightness` | `0` | `-100` (darker) to `100` (brighter) |
| `CornerRadius` | `6` | `0` for square corners |
| `[Playback] Volume` | `50` | `0` to `100` |
| `Loop` | `false` | repeat the video |
| `Url` | empty | starting text of the panel's link box |
| `YouTubeMaxHeight` | `720` | highest YouTube quality (`0` = best); lower uses less CPU |
| `ResumeOnStart` | `true` | play the last video when a save loads |
| `[Hotkeys] TogglePanel`, `TogglePause`, `ToggleVisible` | `[`, `]`, `\` | key names (`F8`, `Home`, `K`, `Numpad5`, ...) or DirectInput codes |

The game log `Documents/My Games/Skyrim Special Edition/SKSE/PiPPlayer.log` warns if one of your keys is also bound
to a game control.

## YouTube notes

- A YouTube link takes a few seconds to start while yt-dlp looks it up.
- Private, age-restricted, and members-only videos do not play (there is no sign-in).
- YouTube changes often break older yt-dlp versions. If YouTube links stop working, press **Update yt-dlp** in the
  control panel.
- yt-dlp streams the video for personal viewing; downloading from YouTube is against YouTube's terms of service.

## Resume

Every video picks up where you left it. The last 100 videos and their positions are kept in
`Documents/My Games/Skyrim Special Edition/SKSE/PiPPlayer_History.txt`. Positions in the first 10 seconds or the last
15 seconds start the video over.

## Compatibility

- **Game versions:** Skyrim SE/AE 1.5.97, 1.6.x, 1.7.x; tested on Enderal SE (1.7.104). Not for Skyrim VR or LE.
- **Enderal SE:** supported; Enderal already includes Address Library.
- **macOS (CrossOver):** supported and tested. Video decodes on the CPU there; lower `YouTubeMaxHeight` if the game
  slows down.
- **SSE Display Tweaks:** compatible.
- **ENB, ReShade, Community Shaders:** not tested yet; reports welcome.
- Load order: none (no ESP).

## Known limitations

- Video controls appear while the control panel is open, not over game menus.
- No gamepad support yet.
- An audio-only file shows "Buffering..." because it has no picture.

## Troubleshooting

- **Nothing happens when I press `[`:** check `Documents/My Games/Skyrim Special Edition/SKSE/skse64.log` for
  `PiPPlayer.dll ... loaded correctly`. If it is missing, check SKSE and Address Library.
- **A key does something in the game too:** see `PiPPlayer.log` for conflicts and pick another key in the INI.
- **YouTube says it needs the YouTube tools:** see "Getting YouTube support" above.
- **YouTube stopped working:** press **Update yt-dlp** in the control panel.
- Anything else: attach `PiPPlayer.log` when you report it.

## Uninstall

Remove the mod in your mod manager, or delete `PiPPlayer.dll`, `PiPPlayer.ini`, and the `PiPPlayer` folder from
`Data/SKSE/Plugins`. Safe at any time.

## Credits and licenses

PiP Player is MIT licensed. It uses:

- [mpv / libmpv](https://mpv.io) (LGPL; separate, replaceable `libmpv-2.dll`, built by
  [zhongfly/mpv-winbuild](https://github.com/zhongfly/mpv-winbuild))
- [yt-dlp](https://github.com/yt-dlp/yt-dlp) (Unlicense) and [QuickJS-NG](https://github.com/quickjs-ng/quickjs) (MIT)
- [CommonLibSSE-GG](https://github.com/eddoursul/CommonLibSSE-GG) (MIT), [Dear ImGui](https://github.com/ocornut/imgui)
  (MIT), [spdlog](https://github.com/gabime/spdlog) (MIT), [simpleini](https://github.com/brofield/simpleini) (MIT),
  DirectXMath/DirectXTK (MIT), [xbyak](https://github.com/herumi/xbyak) (BSD-3),
  [rapidcsv](https://github.com/d99kris/rapidcsv) (BSD-3)

Full notices are in `Data/SKSE/Plugins/PiPPlayer/THIRD_PARTY_NOTICES.txt`. Source:
[github.com/anuvgupta/skypip](https://github.com/anuvgupta/skypip).
