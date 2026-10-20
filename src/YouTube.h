#pragma once

namespace PiP::YouTube
{
	// YouTube (and other site) links play through the bundled yt-dlp.exe, run by libmpv's ytdl hook.
	[[nodiscard]] bool                         Available();
	[[nodiscard]] bool                         IsYouTubeLink(std::string_view a_source);
	[[nodiscard]] const std::filesystem::path& Executable();

	// yt-dlp format selection up to a_maxHeight pixels (0 = best), preferring H.264, which is cheapest to decode.
	[[nodiscard]] std::string FormatFor(int a_maxHeight);

	// Runs "yt-dlp.exe -U" in the background; UpdateStatus() reports progress and the result.
	void                      StartUpdate();
	[[nodiscard]] bool        IsUpdating();
	[[nodiscard]] std::string UpdateStatus();
}
