# PiP Player

Watch YouTube videos, livestreams, or video files from your PC in a small window in the corner of the screen while you
play Skyrim Special Edition or Enderal SE.

- Works in fullscreen and windowed mode, on Windows and on a Mac with CrossOver.
- Picks up each video where you left off, even after you quit the game.
- Just an SKSE plugin: nothing to enable in your load order, nothing saved in your save files. You can add or remove it
  at any time.

## Install

You need [SKSE](https://skse.silverlock.org/) and
[Address Library for SKSE Plugins](https://www.nexusmods.com/skyrimspecialedition/mods/32444). Enderal SE already
comes with Address Library.

1. Go to the [latest release](https://github.com/anuvgupta/skypip/releases/latest) and download
   **`PiPPlayer-<version>-with-YouTube.zip`**.
2. Install it like any other mod with Mod Organizer 2 or Vortex. Or, without a mod manager, extract it into your
   game's `Data` folder (the `Data` folder next to `SkyrimSE.exe`).
3. Start the game through SKSE.

**Don't want YouTube?** Download `PiPPlayer-<version>.zip` instead. It is the same mod without the YouTube tools, so it
plays only video files and direct video links.

### Adding YouTube later

If you installed `PiPPlayer-<version>.zip` and change your mind, install `PiPPlayer-YouTube-Tools-<version>.zip` from
the same release. It only adds the YouTube tools.

Or get the two tools yourself and put them in `Data/SKSE/Plugins/PiPPlayer`, the folder that contains `libmpv-2.dll`:

1. Download `yt-dlp_win.zip` from [yt-dlp's releases](https://github.com/yt-dlp/yt-dlp/releases/latest) and extract
   everything in it into that folder (you should end up with `yt-dlp.exe` and an `_internal` folder there).
2. Download `qjs-windows-x86_64.exe` from [QuickJS-NG's releases](https://github.com/quickjs-ng/quickjs/releases/latest),
   put it in that folder, and rename it to `qjs.exe`.

## Watch your first video

1. Load a save and press **`[`**. The game pauses, a mouse cursor appears, and the control panel opens next to the
   video area.
2. Click the box at the top of the panel, paste a link with **Ctrl+V**, and press **Play**. Any of these work:
   - a YouTube link, like `https://www.youtube.com/watch?v=aqz-KE-bpKQ`
   - a link that ends in a video file, like `.mp4` or `.webm`
   - a video file on your PC, like `C:\Users\you\Videos\clip.mp4`
3. Press **`[`** or **Esc** to close the panel. The game unpauses and the video keeps playing in the corner.

YouTube videos take a few seconds to start. Only public videos work: private, age-restricted, and members-only videos
do not play.

## Keys

| Key | What it does |
|---|---|
| `[` | Open or close the control panel |
| `]` | Pause or resume the video |
| `\` | Show or hide the video |

These are the three keys to the right of P on a US keyboard. On other keyboard layouts, use the same three keys even if
they are labeled differently. To use other keys, see [Settings file](#settings-file).

## The control panel

Open it with `[`. Everything you change here is saved automatically.

- **Play / Play clipboard**: play the link in the box, or the link you last copied.
- **Recent**: your last 100 videos. Pick one and press **Resume** to continue where you left off.
- **Pause, Stop, Volume**.
- **Quality**: switch the playing YouTube video's quality. Lower quality is easier on your computer.
- **YouTube lookup**: leave it on **Reliable**. **Faster** starts YouTube videos about a second sooner, but a few
  videos may then take longer while it retries.
- **Update yt-dlp**: press this if YouTube videos stop playing. YouTube changes often, and an update usually fixes it.
- **Corner, Size, Opacity, Brightness, Margins**: where the video sits and how it looks.

While the panel is open, the video also has its own controls: a play/pause button, 5 second skip buttons, and a seek
bar. Click the video to pause it, or drag it to another corner.

## Picking up where you left off

Every video starts where you stopped it last time. When you load a save, your last video starts playing again
automatically. To turn that off, see [Settings file](#settings-file).

## Troubleshooting

- **Nothing happens when I press `[`.** Make sure you started the game through SKSE, and that Address Library is
  installed.
- **A key also does something in the game.** Change the key in the [settings file](#settings-file).
- **YouTube links say I need the YouTube tools.** You installed the version without YouTube; see
  [Adding YouTube later](#adding-youtube-later).
- **YouTube videos stopped playing.** Open the panel and press **Update yt-dlp**.
- **Something else.** The log is `Documents/My Games/Skyrim Special Edition/SKSE/PiPPlayer.log`; include it when you
  report a problem.

## Settings file

Most settings are in the control panel. A few more are in `Data/SKSE/Plugins/PiPPlayer.ini`, which you can open with
Notepad. Close the game before editing it.

| Setting | What it does |
|---|---|
| `ResumeOnStart=true` | Set to `false` to stop your last video from playing when you load a save. |
| `Loop=false` | Set to `true` to replay a video when it ends. |
| `CornerRadius=6` | How rounded the video's corners are. `0` makes them square. |
| `TogglePanel=[`, `TogglePause=]`, `ToggleVisible=\` | The keys. Use a key name such as `F8`, `Home`, `K`, or `Numpad5`. |

The file also contains every setting from the control panel, with a short explanation of each.

## Uninstall

Remove the mod in your mod manager, or delete `PiPPlayer.dll`, `PiPPlayer.ini`, and the `PiPPlayer` folder from
`Data/SKSE/Plugins`. You can do this at any time.

## For developers

### Build on macOS (cross-compile)

```bash
brew install llvm lld cmake ninja xwin
xwin --accept-license splat --use-winsysroot-style --preserve-ms-arch-notation --output ~/skyrim-toolchain/msvc
export XWIN_ROOT=~/skyrim-toolchain/msvc
cmake --preset mac-release
cmake --build --preset mac-release
scripts/package.sh
```

`xwin --accept-license` accepts Microsoft's license for the MSVC CRT and Windows SDK files it downloads.
`scripts/package.sh` writes the three release archives to `build/package`, and `scripts/deploy-crossover.sh` copies
them into a CrossOver bottle (default bottle `Steam`, Enderal SE).

### Build on Windows

From a Visual Studio 2022 x64 developer prompt:

```bat
cmake --preset msvc-release
cmake --build --preset msvc-release
```

Package with `scripts/package.sh msvc-release` from Git Bash.

## License

MIT (see `LICENSE`). Bundled third-party software is listed in `dist/THIRD_PARTY_NOTICES.txt`; libmpv is LGPL.
