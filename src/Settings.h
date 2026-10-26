#pragma once

namespace PiP
{
	enum class Corner
	{
		TopLeft,
		TopRight,
		BottomLeft,
		BottomRight
	};

	// How YouTube lookups use QuickJS, which solves YouTube's JavaScript challenge for yt-dlp.
	enum class YouTubeSolver
	{
		Always,    // every lookup (yt-dlp's supported setup)
		Fallback,  // only to retry a lookup that failed without it (faster)
		Off        // never (fastest; some videos may fail or miss qualities)
	};

	// Values from Data/SKSE/Plugins/PiPPlayer.ini. Layout and volume can change in game (control panel) and are
	// written back with Save().
	struct Settings
	{
		// [Overlay] - pixels are at the game's render resolution
		Corner corner = Corner::BottomLeft;
		int    width = 480;
		int    marginX = 24;
		int    marginY = 24;
		float  opacity = 1.0F;
		float  cornerRadius = 6.0F;
		int    brightness = 0;  // -100 (darker) to 100 (brighter); 0 = unchanged

		// [Playback]
		int         volume = 50;
		bool        loop = false;
		std::string url;                 // initial text of the control panel's URL box
		int         youTubeMaxHeight = 720;  // highest YouTube resolution to request; 0 = best
		bool          resumeOnStart = true;  // play the last video, from where it left off, once a save loads
		YouTubeSolver youTubeSolver = YouTubeSolver::Always;
		bool          pauseForDialogue = true;  // pause during conversations
		int           resumeAfterDialogue = 3;  // seconds after a conversation ends; 0 = do not resume then
		int           resumeAfterSilence = 60;  // seconds without a spoken line; 0 = do not resume then

		// [Hotkeys] - DirectInput scan codes, set in the INI by name (see Keys.h)
		std::uint32_t togglePanelKey = 0x1A;    // [
		std::uint32_t togglePauseKey = 0x1B;    // ]
		std::uint32_t toggleVisibleKey = 0x2B;  // backslash

		static void Load();
		static void Save();  // writes changed layout and volume back to the INI

		// Safe from any thread.
		[[nodiscard]] static Settings Get();
		static void                   Update(const std::function<void(Settings&)>& a_edit);
	};
}
