# PiP Player

A picture-in-picture video player for Skyrim Special Edition and Enderal SE. Copy a video link, press a hotkey, and
it plays in a corner of the screen while you play. It is drawn inside the game's own frame, so it works in fullscreen
and borderless modes, on Windows and under CrossOver on macOS.

It is a pure SKSE plugin: no ESP, nothing stored in saves, safe to add or remove at any time.

## Requirements

- SKSE64 for your game version
- Address Library for SKSE Plugins (Enderal SE already ships it; Skyrim SE users install it from Nexus)
- Skyrim SE 1.5.97, 1.6.x or 1.7.x (tested target: Enderal SE on 1.7.104)

## Install

Each release has three archives. Their root is the game's `Data` folder: install them with Mod Organizer 2 or Vortex,
or extract them into `Data` (the folder next to `SkyrimSE.exe`).

| Archive | Contents | Plays |
|---|---|---|
| `PiPPlayer-<version>-with-YouTube.zip` | plugin, libmpv, yt-dlp, QuickJS | everything, including YouTube |
| `PiPPlayer-<version>.zip` | plugin, libmpv | local files and direct links |
| `PiPPlayer-YouTube-Tools-<version>.zip` | yt-dlp, QuickJS | adds YouTube to the plain archive |

For YouTube with the plain archive, either install the YouTube Tools archive too, or add these to
`Data/SKSE/Plugins/PiPPlayer/` yourself:

1. `yt-dlp_win.zip` from [yt-dlp releases](https://github.com/yt-dlp/yt-dlp/releases/latest) (tested: 2026.08.19),
   extracted so `yt-dlp.exe` and its `_internal` folder are in that folder (the single-file `yt-dlp.exe` also works
   but starts slower)
2. `qjs-windows-x86_64.exe` from [QuickJS-NG releases](https://github.com/quickjs-ng/quickjs/releases/latest)
   (tested: v0.17.0), renamed to `qjs.exe`

Installed files:

```
Data/SKSE/Plugins/PiPPlayer.dll
Data/SKSE/Plugins/PiPPlayer.ini
Data/SKSE/Plugins/PiPPlayer/libmpv-2.dll
Data/SKSE/Plugins/PiPPlayer/yt-dlp.exe      (YouTube)
Data/SKSE/Plugins/PiPPlayer/_internal/      (YouTube, yt-dlp's files)
Data/SKSE/Plugins/PiPPlayer/qjs.exe         (YouTube)
```

## Use

| Key | Action |
|---|---|
| `[` | Open or close the control panel |
| `]` | Pause or resume |
| `\` | Show or hide the player |

These are the three keys right of P on a US keyboard; on other layouts the same physical keys are used. Remap them in
the INI by name (`F8`, `Home`, `K`, `Numpad5`, ...).

The control panel opens beside the video and pauses the game while it is open (Esc, `[`, or its X closes it):

- paste a URL or file path into the box and press Play, or press **Play clipboard**
- pick a video from **Recent** (the last one is preselected) and press **Resume** to continue where you left it
- pause, stop, and volume
- corner, size, opacity, and margins, applied live and saved to the INI

While the panel is open the video shows a play/pause button, 5 second skip buttons, and a seek bar. Click the video
to pause it, or drag it to move it to another corner.

Plays YouTube links (videos and live streams), direct video links (`.mp4`, `.webm`, HLS streams, ...), and local
files. YouTube links take a few seconds to start while the bundled yt-dlp looks up the video; private, age-restricted,
and members-only videos do not play. The panel's **Quality** list switches a playing YouTube video between the qualities it offers within a second or two
(the choice also becomes the default). Use **Update yt-dlp** in the control panel when YouTube links stop working
(YouTube changes often break older yt-dlp versions). yt-dlp is used for personal playback; downloading from YouTube is
against YouTube's terms of service.

### Resume

Every video resumes from where you left it: the last 100 videos and their positions are kept in
`Documents/My Games/Skyrim Special Edition/SKSE/PiPPlayer_History.txt`. Positions in the first 10 seconds or the last
15 seconds start the video over. When a save loads, the last video picks up where it left off (`ResumeOnStart`).

## Settings

Edit `Data/SKSE/Plugins/PiPPlayer.ini` while the game is closed, or use the control panel (it saves layout and volume
back to the INI).

| Setting | Default | |
|---|---|---|
| `[Overlay] Corner` | `BottomLeft` | `TopLeft`, `TopRight`, `BottomLeft`, `BottomRight` |
| `Width` | `480` | pixels at the game's resolution; height follows the video |
| `MarginX`, `MarginY` | `24` | distance from the screen edges |
| `Opacity` | `1.0` | `0.05` to `1.0` |
| `Brightness` | `0` | `-100` (darker) to `100` (brighter) |
| `CornerRadius` | `6` | `0` for square corners |
| `[Playback] Volume` | `50` | `0` to `100` |
| `Loop` | `false` | repeat the video |
| `Url` | empty | initial text of the control panel's URL box |
| `YouTubeMaxHeight` | `720` | highest YouTube resolution requested (`0` = best); lower is lighter on the CPU |
| `YouTubeSolver` | `Always` | QuickJS for YouTube lookups: `Always`, `Fallback` (retry with it only when a lookup fails; faster), or `Off` |
| `ResumeOnStart` | `true` | once a save loads, play the last video from where it left off |
| `[Hotkeys] TogglePanel`, `TogglePause`, `ToggleVisible` | `[`, `]`, `\` | key names or DirectInput scan codes; the log warns when a key is also bound to a game control |

The log is `Documents/My Games/Skyrim Special Edition/SKSE/PiPPlayer.log`.

## Build

### macOS (cross-compile)

```bash
brew install llvm lld cmake ninja xwin
xwin --accept-license splat --use-winsysroot-style --preserve-ms-arch-notation --output ~/skyrim-toolchain/msvc
export XWIN_ROOT=~/skyrim-toolchain/msvc
cmake --preset mac-release
cmake --build --preset mac-release
scripts/package.sh
```

`scripts/package.sh` writes the three release archives to `build/package`.

`xwin --accept-license` accepts Microsoft's license for the MSVC CRT and Windows SDK files it downloads.
`scripts/deploy-crossover.sh` copies the staged files into a CrossOver bottle (default bottle `Steam`, Enderal SE).

### Windows

From a Visual Studio 2022 x64 developer prompt:

```bat
cmake --preset msvc-release
cmake --build --preset msvc-release
```

Package with `scripts/package.sh msvc-release` from Git Bash.

## License

MIT (see `LICENSE`). Bundled third-party software is listed in `dist/THIRD_PARTY_NOTICES.txt`; libmpv is LGPL.
