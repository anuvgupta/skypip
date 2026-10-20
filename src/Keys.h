#pragma once

namespace PiP::Keys
{
	// Keyboard keys are DirectInput scan codes (physical positions, as reported by ButtonEvent::GetIDCode).

	// A key name ("[", "Backslash", "F8", "Home", "Numpad5", case-insensitive) or a numeric code (decimal or 0x hex).
	std::optional<std::uint32_t> ParseKey(std::string_view a_text);

	// Display name for a code, e.g. "[" or "Home"; "key <n>" when unnamed.
	std::string KeyName(std::uint32_t a_code);
}
