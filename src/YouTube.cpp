#include "YouTube.h"

#include "Paths.h"

namespace PiP::YouTube
{
	namespace
	{
		std::atomic_bool g_updating{ false };
		std::mutex       g_statusMutex;
		std::string      g_status;

		void SetStatus(std::string a_status)
		{
			std::scoped_lock lock(g_statusMutex);
			g_status = std::move(a_status);
		}

		// Runs a_commandLine without a console window and returns its exit code and combined output.
		std::optional<std::pair<DWORD, std::string>> Run(std::wstring a_commandLine, const std::filesystem::path& a_directory)
		{
			SECURITY_ATTRIBUTES inherit{ sizeof(inherit), nullptr, TRUE };
			HANDLE              read = nullptr;
			HANDLE              write = nullptr;
			if (!::CreatePipe(&read, &write, &inherit, 0)) {
				return std::nullopt;
			}
			::SetHandleInformation(read, HANDLE_FLAG_INHERIT, 0);

			STARTUPINFOW startup{ .cb = sizeof(startup) };
			startup.dwFlags = STARTF_USESTDHANDLES;
			startup.hStdOutput = write;
			startup.hStdError = write;
			PROCESS_INFORMATION process{};
			const bool started = ::CreateProcessW(nullptr, a_commandLine.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr,
				a_directory.c_str(), &startup, &process);
			::CloseHandle(write);
			if (!started) {
				::CloseHandle(read);
				return std::nullopt;
			}

			std::string output;
			char        buffer[4096];
			DWORD       bytes = 0;
			while (::ReadFile(read, buffer, sizeof(buffer), &bytes, nullptr) && bytes > 0) {
				output.append(buffer, bytes);
			}
			::CloseHandle(read);

			::WaitForSingleObject(process.hProcess, INFINITE);
			DWORD exitCode = 1;
			::GetExitCodeProcess(process.hProcess, &exitCode);
			::CloseHandle(process.hProcess);
			::CloseHandle(process.hThread);
			return std::pair{ exitCode, std::move(output) };
		}

		std::string LastLine(std::string_view a_text)
		{
			while (!a_text.empty() && std::isspace(static_cast<unsigned char>(a_text.back()))) {
				a_text.remove_suffix(1);
			}
			const auto start = a_text.find_last_of("\r\n");
			return std::string(start == std::string_view::npos ? a_text : a_text.substr(start + 1));
		}
	}

	bool Available()
	{
		std::error_code error;
		return std::filesystem::is_regular_file(Executable(), error);
	}

	bool IsYouTubeLink(std::string_view a_source)
	{
		std::string lower(a_source);
		std::ranges::transform(lower, lower.begin(), [](unsigned char a_char) { return static_cast<char>(std::tolower(a_char)); });
		return lower.find("youtube.com/") != std::string::npos || lower.find("youtu.be/") != std::string::npos;
	}

	const std::filesystem::path& Executable()
	{
		static const auto path = Paths::PluginFolder() / L"yt-dlp.exe";
		return path;
	}

	std::string FormatFor(int a_maxHeight)
	{
		if (a_maxHeight <= 0) {
			return "bv*[vcodec^=avc1]+ba/bv*+ba/b";
		}
		return std::format("bv*[height<={0}][vcodec^=avc1]+ba/bv*[height<={0}]+ba/b[height<={0}]/b", a_maxHeight);
	}

	void StartUpdate()
	{
		if (!Available() || g_updating.exchange(true)) {
			return;
		}
		SetStatus("Updating yt-dlp...");

		std::thread([] {
			const auto& exe = Executable();
			auto        result = Run(std::format(L"\"{}\" --no-config -U", exe.wstring()), exe.parent_path());
			if (!result) {
				logger::error("Could not start yt-dlp -U ({})", ::GetLastError());
				SetStatus("Could not start yt-dlp.");
			} else {
				const auto& [exitCode, output] = *result;
				logger::info("yt-dlp -U exited with {}:\n{}", exitCode, output);
				const auto summary = LastLine(output);
				SetStatus(summary.empty() ? std::format("yt-dlp update finished (exit code {}).", exitCode) : summary);
			}
			g_updating = false;
		}).detach();
	}

	bool IsUpdating()
	{
		return g_updating;
	}

	std::string UpdateStatus()
	{
		std::scoped_lock lock(g_statusMutex);
		return g_status;
	}
}
