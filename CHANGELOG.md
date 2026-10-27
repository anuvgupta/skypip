# Changelog

## 1.0.8 - 2026-10-26

- PiP Player is now called SkyPiP, and the downloads are named `SkyPiP-<version>-with-YouTube.zip`, `SkyPiP-<version>.zip`,
  and `SkyPiP-YouTube-Tools-<version>.zip`. The installed files keep their `PiPPlayer` names, so your settings and Recent
  list carry over.
- YouTube videos no longer freeze on one picture while the sound plays on. YouTube cuts long downloads, and the
  player now downloads videos in smaller pieces and retries longer before giving up.
- If a YouTube download still breaks off mid-video, the player reloads the video where it was, as the **Reload** button
  does, instead of leaving the picture frozen.

## 1.0.7 - 2026-10-26

- When you load a save, the last video only starts again if it was still playing or paused when you quit the game. If
  you had stopped it, or it had finished, nothing plays.

## 1.0.6 - 2026-10-26

- New defaults: the video is 600 px wide (was 480), 92% opaque (was 96%), and YouTube videos play at up to 1080p
  (was 720p). Existing `PiPPlayer.ini` files keep their settings.

## 1.0.5 - 2026-10-26

- New **Reset to defaults** button in the control panel puts every setting on the panel back to its default.
- The video is now 96% opaque by default (was 100%). Existing `PiPPlayer.ini` files keep their setting.
- New conversation defaults: the video resumes 8 seconds after a conversation ends (was 3), or once no one has spoken
  for 15 seconds (was 9). Existing `PiPPlayer.ini` files keep their settings.
- When a YouTube video can't play, the panel shows YouTube's actual reason (for example "Sign in to confirm you're not
  a bot") instead of a generic error.

## 1.0.4 - 2026-10-25

- The video pauses while you talk to someone and resumes 3 seconds after the conversation ends, or once no one has
  spoken for 9 seconds. Both times are adjustable (or can be turned off) in the control panel's new **Conversations**
  section and in the INI (`PauseForDialogue`, `ResumeAfterDialogue`, `ResumeAfterSilence`).
- **Reload** button in the control panel: loads the current video again from its link or file and continues where it
  was (from the start if it had ended).
- New installs start with the video in the top-right corner at 75% volume (was bottom-left, 50%). Existing
  `PiPPlayer.ini` files keep their settings.
- Control panel settings are saved two seconds after you change them, not only when the panel closes.

## 1.0.4-beta - 2026-10-25

- The video pauses while you talk to someone and resumes 3 seconds after the conversation ends, or after a minute in
  which no one speaks. Both are adjustable (or can be turned off) in the control panel's new **Conversations** section
  and in the INI (`PauseForDialogue`, `ResumeAfterDialogue`, `ResumeAfterSilence`).

## 1.0.3 - 2026-10-19

- The control panel calls the YouTube setting **YouTube lookup** with the choices **Reliable** (default), **Faster**,
  and **Fastest** instead of JS solver / Always / Fallback / Off. The INI values are unchanged.
- Rewritten README and mod page: which download to pick, how to watch your first video, the keys, the control panel,
  and troubleshooting.

## 1.0.2 - 2026-10-19

- New `YouTubeSolver` setting (INI and control panel) for how YouTube lookups use QuickJS: `Always` (default),
  `Fallback` (look up without it and retry a failed lookup with it; about 1-2 s faster under CrossOver), or `Off`.
- Known issue: under CrossOver a brief click can still be heard while a YouTube link is looked up.

## 1.0.1 - 2026-10-19

- The player fades in and out when shown or hidden instead of appearing and disappearing instantly.
- No more audio pops when starting, switching, or stopping videos: the audio device stays open in one format
  (48 kHz stereo) and the volume ramps around video changes.
- Brightness control in the control panel and the INI (`Brightness`, -100 to 100).
- Bundles yt-dlp's folder build (`yt-dlp.exe` + `_internal`), which starts faster than the single-file executable.
- Known issue: under CrossOver a brief click can be heard while a YouTube link is looked up.

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
