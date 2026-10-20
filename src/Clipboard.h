#pragma once

namespace PiP::Clipboard
{
	// The clipboard text as a playable source (UTF-8): an http(s) URL or an existing file path, else nullopt.
	std::optional<std::string> ReadVideoSource();
}
