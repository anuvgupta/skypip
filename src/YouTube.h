#pragma once

namespace PiP::YouTube
{
	// YouTube (and other site) links play through the bundled yt-dlp.exe, run by libmpv's ytdl hook.
	[[nodiscard]] bool                         Available();
	[[nodiscard]] bool                         IsYouTubeLink(std::string_view a_source);
	[[nodiscard]] const std::filesystem::path& Executable();

	// A cookies.txt export of a signed-in YouTube account, for videos YouTube shows only signed in ("Sign in to confirm
	// you're not a bot", age-restricted videos). It sits next to the log in My Games, so updating the mod keeps it.
	[[nodiscard]] std::optional<std::filesystem::path> CookiesPath();
	[[nodiscard]] bool                                 HasCookies();  // checked at most every couple of seconds

	// yt-dlp format selection up to a_maxHeight pixels (0 = best), preferring H.264, which is cheapest to decode.
	[[nodiscard]] std::string FormatFor(int a_maxHeight);

	// Runs "yt-dlp.exe -U" in the background; UpdateStatus() reports progress and the result.
	void                      StartUpdate();
	[[nodiscard]] bool        IsUpdating();
	[[nodiscard]] std::string UpdateStatus();
}
