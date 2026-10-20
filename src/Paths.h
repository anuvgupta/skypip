#pragma once

namespace PiP::Paths
{
	// Data/SKSE/Plugins/PiPPlayer: the folder next to PiPPlayer.dll holding libmpv, yt-dlp, and QuickJS.
	[[nodiscard]] const std::filesystem::path& PluginFolder();

	[[nodiscard]] std::string ToUtf8(const std::filesystem::path& a_path);
}
