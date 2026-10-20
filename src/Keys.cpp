#include "Keys.h"

#include <cctype>
#include <charconv>

namespace PiP::Keys
{
	namespace
	{
		struct NamedKey
		{
			std::string_view name;
			std::uint32_t    code;
		};

		// The first entry for a code is its display name; later entries are aliases.
		constexpr NamedKey kKeys[]{
			{ "Escape", 0x01 }, { "1", 0x02 }, { "2", 0x03 }, { "3", 0x04 }, { "4", 0x05 }, { "5", 0x06 }, { "6", 0x07 },
			{ "7", 0x08 }, { "8", 0x09 }, { "9", 0x0A }, { "0", 0x0B }, { "-", 0x0C }, { "=", 0x0D }, { "Backspace", 0x0E },
			{ "Tab", 0x0F }, { "Q", 0x10 }, { "W", 0x11 }, { "E", 0x12 }, { "R", 0x13 }, { "T", 0x14 }, { "Y", 0x15 },
			{ "U", 0x16 }, { "I", 0x17 }, { "O", 0x18 }, { "P", 0x19 }, { "[", 0x1A }, { "]", 0x1B }, { "Enter", 0x1C },
			{ "LeftCtrl", 0x1D }, { "A", 0x1E }, { "S", 0x1F }, { "D", 0x20 }, { "F", 0x21 }, { "G", 0x22 }, { "H", 0x23 },
			{ "J", 0x24 }, { "K", 0x25 }, { "L", 0x26 }, { ";", 0x27 }, { "'", 0x28 }, { "`", 0x29 }, { "LeftShift", 0x2A },
			{ "\\", 0x2B }, { "Z", 0x2C }, { "X", 0x2D }, { "C", 0x2E }, { "V", 0x2F }, { "B", 0x30 }, { "N", 0x31 },
			{ "M", 0x32 }, { ",", 0x33 }, { ".", 0x34 }, { "/", 0x35 }, { "RightShift", 0x36 }, { "NumpadMultiply", 0x37 },
			{ "LeftAlt", 0x38 }, { "Space", 0x39 }, { "CapsLock", 0x3A }, { "F1", 0x3B }, { "F2", 0x3C }, { "F3", 0x3D },
			{ "F4", 0x3E }, { "F5", 0x3F }, { "F6", 0x40 }, { "F7", 0x41 }, { "F8", 0x42 }, { "F9", 0x43 }, { "F10", 0x44 },
			{ "NumLock", 0x45 }, { "ScrollLock", 0x46 }, { "Numpad7", 0x47 }, { "Numpad8", 0x48 }, { "Numpad9", 0x49 },
			{ "NumpadMinus", 0x4A }, { "Numpad4", 0x4B }, { "Numpad5", 0x4C }, { "Numpad6", 0x4D }, { "NumpadPlus", 0x4E },
			{ "Numpad1", 0x4F }, { "Numpad2", 0x50 }, { "Numpad3", 0x51 }, { "Numpad0", 0x52 }, { "NumpadPeriod", 0x53 },
			{ "F11", 0x57 }, { "F12", 0x58 }, { "NumpadEnter", 0x9C }, { "RightCtrl", 0x9D }, { "NumpadDivide", 0xB5 },
			{ "RightAlt", 0xB8 }, { "Pause", 0xC5 }, { "Home", 0xC7 }, { "Up", 0xC8 }, { "PageUp", 0xC9 }, { "Left", 0xCB },
			{ "Right", 0xCD }, { "End", 0xCF }, { "Down", 0xD0 }, { "PageDown", 0xD1 }, { "Insert", 0xD2 }, { "Delete", 0xD3 },

			// Aliases: word names for symbols (clearer in an INI file) and common spellings.
			{ "LeftBracket", 0x1A }, { "RightBracket", 0x1B }, { "Backslash", 0x2B }, { "Semicolon", 0x27 },
			{ "Apostrophe", 0x28 }, { "Grave", 0x29 }, { "Tilde", 0x29 }, { "Comma", 0x33 }, { "Period", 0x34 },
			{ "Slash", 0x35 }, { "Minus", 0x0C }, { "Equals", 0x0D }, { "Esc", 0x01 }, { "Return", 0x1C }, { "Ins", 0xD2 },
			{ "Del", 0xD3 }, { "PgUp", 0xC9 }, { "PgDn", 0xD1 }, { "PageDn", 0xD1 },
		};

		bool EqualsIgnoreCase(std::string_view a_lhs, std::string_view a_rhs)
		{
			return std::ranges::equal(a_lhs, a_rhs, [](char a_l, char a_r) {
				return std::tolower(static_cast<unsigned char>(a_l)) == std::tolower(static_cast<unsigned char>(a_r));
			});
		}

		std::optional<std::uint32_t> ParseNumber(std::string_view a_text)
		{
			int base = 10;
			if (a_text.size() > 2 && a_text[0] == '0' && (a_text[1] == 'x' || a_text[1] == 'X')) {
				a_text.remove_prefix(2);
				base = 16;
			}
			std::uint32_t value = 0;
			const auto [end, error] = std::from_chars(a_text.data(), a_text.data() + a_text.size(), value, base);
			if (error != std::errc{} || end != a_text.data() + a_text.size() || value == 0 || value > 0xFF) {
				return std::nullopt;
			}
			return value;
		}
	}

	std::optional<std::uint32_t> ParseKey(std::string_view a_text)
	{
		while (!a_text.empty() && std::isspace(static_cast<unsigned char>(a_text.front()))) {
			a_text.remove_prefix(1);
		}
		while (!a_text.empty() && std::isspace(static_cast<unsigned char>(a_text.back()))) {
			a_text.remove_suffix(1);
		}
		if (a_text.empty()) {
			return std::nullopt;
		}

		for (const auto& key : kKeys) {
			if (EqualsIgnoreCase(key.name, a_text)) {
				return key.code;
			}
		}
		// After the name lookup, so "1".."9" and "0" mean the number-row keys rather than raw codes.
		return ParseNumber(a_text);
	}

	std::string KeyName(std::uint32_t a_code)
	{
		for (const auto& key : kKeys) {
			if (key.code == a_code) {
				return std::string(key.name);
			}
		}
		return std::format("key {}", a_code);
	}
}
