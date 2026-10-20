#include "Clipboard.h"

namespace PiP::Clipboard
{
	namespace
	{
		std::optional<std::wstring> ReadText()
		{
			if (!::OpenClipboard(nullptr)) {
				logger::warn("OpenClipboard failed ({})", ::GetLastError());
				return std::nullopt;
			}

			std::optional<std::wstring> text;
			if (const HANDLE data = ::GetClipboardData(CF_UNICODETEXT)) {
				if (const auto* chars = static_cast<const wchar_t*>(::GlobalLock(data))) {
					text.emplace(chars);
					::GlobalUnlock(data);
				}
			}
			::CloseClipboard();
			return text;
		}

		std::wstring_view Trim(std::wstring_view a_text)
		{
			constexpr std::wstring_view junk = L" \t\r\n\"'<>";
			const auto first = a_text.find_first_not_of(junk);
			if (first == std::wstring_view::npos) {
				return {};
			}
			const auto last = a_text.find_last_not_of(junk);
			return a_text.substr(first, last - first + 1);
		}

		bool IsWebUrl(std::wstring_view a_text)
		{
			for (const auto scheme : { L"http://"sv, L"https://"sv }) {
				if (a_text.size() > scheme.size() && _wcsnicmp(a_text.data(), scheme.data(), scheme.size()) == 0) {
					return true;
				}
			}
			return false;
		}
	}

	std::optional<std::string> ReadVideoSource()
	{
		const auto text = ReadText();
		if (!text) {
			return std::nullopt;
		}

		const auto candidate = Trim(*text);
		if (candidate.empty() || candidate.find_first_of(L"\r\n") != std::wstring_view::npos) {
			return std::nullopt;
		}

		std::error_code error;
		if (!IsWebUrl(candidate) && !std::filesystem::is_regular_file(std::filesystem::path(candidate), error)) {
			return std::nullopt;
		}
		return SKSE::stl::utf16_to_utf8(candidate);
	}
}
