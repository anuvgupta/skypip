# PiP Player - watch videos while you play

## About

Watch YouTube videos, livestreams, or video files from your PC in a small window in the corner of the screen while you
play Skyrim or Enderal.

- Works in fullscreen and windowed mode, on Windows and on a Mac with CrossOver.
- A control panel that pauses the game while you pick a video.
- Pauses the video while you talk to someone, and starts it again afterwards.
- Picks up each video where you left off, even after you quit the game, and plays your last video again when you load
  a save.
- Change the corner, size, opacity, brightness, volume, and YouTube quality any time.
- Just an SKSE plugin: nothing to enable in your load order, nothing saved in your save files. You can add or remove it
  at any time.

## Requirements

- [SKSE](https://skse.silverlock.org/)
- [Address Library for SKSE Plugins](https://www.nexusmods.com/skyrimspecialedition/mods/32444) (Enderal SE already
  comes with it)

## Installation

1. Download **PiPPlayer with YouTube**. If it isn't listed under Files here, download
   `PiPPlayer-<version>-with-YouTube.zip` from [GitHub](https://github.com/anuvgupta/skypip/releases/latest).
2. Install it like any other mod with Mod Organizer 2 or Vortex. Or, without a mod manager, extract it into your
   game's `Data` folder (the `Data` folder next to `SkyrimSE.exe`).
3. Start the game through SKSE.

**Don't want YouTube?** Download **PiPPlayer** instead. It is the same mod without the YouTube tools, so it plays only
video files and direct video links.

### Adding YouTube later

If you installed **PiPPlayer** and change your mind, install **PiPPlayer YouTube Tools** (under Files here, or
`PiPPlayer-YouTube-Tools-<version>.zip` on [GitHub](https://github.com/anuvgupta/skypip/releases/latest)). It only adds
the YouTube tools.

Or get the two tools yourself and put them in `Data/SKSE/Plugins/PiPPlayer`, the folder that contains `libmpv-2.dll`.
With Mod Organizer 2, that folder is inside the PiPPlayer mod: right-click the mod and choose Open in Explorer.

1. Download `yt-dlp_win.zip` from [yt-dlp's releases](https://github.com/yt-dlp/yt-dlp/releases/latest) and extract
   everything in it into that folder (you should end up with `yt-dlp.exe` and an `_internal` folder there).
2. Download `qjs-windows-x86_64.exe` from [QuickJS-NG's releases](https://github.com/quickjs-ng/quickjs/releases/latest),
   put it in that folder, and rename it to `qjs.exe`.

## Watch your first video

1. Load a save and press **Left Bracket**. The game pauses, a mouse cursor appears, and the control panel opens.
2. Click the box at the top of the panel, paste a link with **Ctrl+V**, and press **Play**. Any of these work:
   - a YouTube link
   - a link that ends in a video file, like `.mp4` or `.webm`
   - a video file on your PC, like `C:\Users\you\Videos\clip.mp4`
3. Press **Left Bracket** or **Esc** to close the panel. The game unpauses and the video keeps playing in the corner.

YouTube videos take a few seconds to start. Only public videos work: private, age-restricted, and members-only videos
do not play.

## Keys

| Key | What it does |
|---|---|
| `[` | Open or close the control panel |
| `]` | Pause or resume the video |
| `\` | Show or hide the video |

These are the three keys to the right of P on a US keyboard. On other keyboard layouts, use the same three keys even if
they are labeled differently. You can change them in the settings file (see below).

## The control panel

Open it with **Left Bracket**. Everything you change here is saved automatically.

- **Play / Play clipboard**: play the link in the box, or the link you last copied.
- **Recent**: your last 100 videos. Pick one and press **Resume** to continue where you left off.
- **Pause, Stop, Volume**.
- **Reload**: load the current video again (from its YouTube link, video link, or file) and continue where you were.
  Use it if a video freezes or stops loading.
- **Quality**: switch the playing YouTube video's quality. Lower quality is easier on your computer.
- **YouTube lookup**: leave it on **Reliable**. **Faster** starts YouTube videos about a second sooner, but a few
  videos may then take longer while it retries.
- **Update yt-dlp**: press this if YouTube videos stop playing. YouTube changes often, and an update usually fixes it.
- **Conversations**: the video pauses while you talk to someone and starts again 8 seconds after the conversation
  ends, or once no one has spoken for 15 seconds. Change either time, or set it to off.
- **Corner, Size, Opacity, Brightness, Margins**: where the video sits and how it looks.
- **Reset to defaults**: put every setting on the panel back to how it was when you installed the mod. Your keys and
  the other settings that are only in the settings file stay as they are.

While the panel is open, the video also has its own controls: a play/pause button, 5 second skip buttons, and a seek
bar. Click the video to pause it, or drag it to another corner.

## Settings file

Most settings are in the control panel. A few more are in `Data/SKSE/Plugins/PiPPlayer.ini`, which you can open with
Notepad. Close the game before editing it.

| Setting | What it does |
|---|---|
| `ResumeOnStart=true` | Set to `false` to stop your last video from playing when you load a save. |
| `Loop=false` | Set to `true` to replay a video when it ends. |
| `CornerRadius=6` | How rounded the video's corners are. `0` makes them square. |
| `TogglePanel`, `TogglePause`, `ToggleVisible` | The keys. Use a key name such as `F8`, `Home`, `K`, or `Numpad5`. |

## Compatibility

- **Skyrim Special Edition and Anniversary Edition** and **Enderal SE**. Not for Skyrim VR or the
  original 2011 Skyrim.
- **Mac with CrossOver:** works. Videos use more CPU there; if the game slows down, pick a lower Quality. A faint click
  can sometimes be heard while a YouTube video is being looked up.
- **SSE Display Tweaks:** works.
- **ENB, ReShade, Community Shaders:** not tested yet; let me know how it goes.

## Troubleshooting

- **Nothing happens when I press Left Bracket.** Make sure you started the game through SKSE, and that Address Library
  is installed.
- **A key also does something in the game.** Change the key in the settings file.
- **YouTube links say I need the YouTube tools.** You installed the version without YouTube; see "Adding YouTube
  later".
- **YouTube videos stopped playing.** Open the panel and press **Update yt-dlp**.
- **Something else.** Post in the comments with the log file
  `Documents/My Games/Skyrim Special Edition/SKSE/PiPPlayer.log` attached.

## Uninstall

Remove the mod in your mod manager, or delete `PiPPlayer.dll`, `PiPPlayer.ini`, and the `PiPPlayer` folder from
`Data/SKSE/Plugins`. You can do this at any time.

## Credits and licenses

PiP Player is MIT licensed. It uses:

- [mpv / libmpv](https://mpv.io) (LGPL; separate, replaceable `libmpv-2.dll`, built by
  [zhongfly/mpv-winbuild](https://github.com/zhongfly/mpv-winbuild))
- [yt-dlp](https://github.com/yt-dlp/yt-dlp) (Unlicense) and [QuickJS-NG](https://github.com/quickjs-ng/quickjs) (MIT)
- [CommonLibSSE-GG](https://github.com/eddoursul/CommonLibSSE-GG) (MIT), [Dear ImGui](https://github.com/ocornut/imgui)
  (MIT), [spdlog](https://github.com/gabime/spdlog) (MIT), [simpleini](https://github.com/brofield/simpleini) (MIT),
  DirectXMath/DirectXTK (MIT), [xbyak](https://github.com/herumi/xbyak) (BSD-3),
  [rapidcsv](https://github.com/d99kris/rapidcsv) (BSD-3)

Full notices are in `Data/SKSE/Plugins/PiPPlayer/THIRD_PARTY_NOTICES.txt`. Source code:
[github.com/anuvgupta/skypip](https://github.com/anuvgupta/skypip).
