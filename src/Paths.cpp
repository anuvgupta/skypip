#include "Paths.h"

namespace PiP::Paths
{
	namespace
	{
		std::filesystem::path FindPluginFolder()
		{
			HMODULE self = nullptr;
			::GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
				reinterpret_cast<LPCWSTR>(&FindPluginFolder), &self);

			std::wstring path(MAX_PATH, L'\0');
			for (;;) {
				const auto length = ::GetModuleFileNameW(self, path.data(), static_cast<DWORD>(path.size()));
				if (length < path.size()) {
					path.resize(length);
					break;
				}
				path.resize(path.size() * 2);
			}
			return std::filesystem::path(path).parent_path() / L"PiPPlayer";
		}
	}

	const std::filesystem::path& PluginFolder()
	{
		static const auto folder = FindPluginFolder();
		return folder;
	}

	std::string ToUtf8(const std::filesystem::path& a_path)
	{
		return SKSE::stl::utf16_to_utf8(a_path.wstring()).value_or(std::string{});
	}
}
