#include "Mpv.h"

#include "Paths.h"

namespace PiP::Mpv
{
	namespace
	{
		std::string Win32ErrorText(DWORD a_error)
		{
			char* buffer = nullptr;
			const auto length = ::FormatMessageA(
				FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
				nullptr, a_error, 0, reinterpret_cast<LPSTR>(&buffer), 0, nullptr);
			std::string text = length ? std::string(buffer, length) : std::string{};
			::LocalFree(buffer);
			while (!text.empty() && (text.back() == '\n' || text.back() == '\r' || text.back() == ' ')) {
				text.pop_back();
			}
			return std::format("{} (error {})", text.empty() ? "unknown error" : text, a_error);
		}
	}

	const Api* Load(std::string& a_error)
	{
		static Api api;

		const auto dllPath = Paths::PluginFolder() / L"libmpv-2.dll";
		const auto dllName = Paths::ToUtf8(dllPath);

		const HMODULE library = ::LoadLibraryExW(dllPath.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
		if (!library) {
			a_error = std::format("Could not load {}: {}", dllName, Win32ErrorText(::GetLastError()));
			return nullptr;
		}

#define PIP_MPV_RESOLVE(a_name)                                                                    \
	api.a_name = reinterpret_cast<decltype(api.a_name)>(::GetProcAddress(library, #a_name));       \
	if (!api.a_name) {                                                                             \
		a_error = std::format("{} does not export {}", dllName, #a_name);                         \
		return nullptr;                                                                            \
	}
		PIP_MPV_FUNCTIONS(PIP_MPV_RESOLVE)
#undef PIP_MPV_RESOLVE

		const auto version = api.mpv_client_api_version();
		if ((version >> 16) != (MPV_CLIENT_API_VERSION >> 16)) {
			a_error = std::format("{} has client API {}.{}, expected {}.x", dllName, version >> 16, version & 0xFFFF,
				MPV_CLIENT_API_VERSION >> 16);
			return nullptr;
		}

		logger::info("Loaded {} (client API {}.{})", dllName, version >> 16, version & 0xFFFF);
		return &api;
	}
}
