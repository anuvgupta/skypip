#include "History.h"

#include <cctype>
#include <charconv>
#include <cmath>
#include <fstream>

namespace PiP::History
{
	namespace
	{
		constexpr std::size_t kMaxEntries = 100;
		constexpr double      kStartGrace = 10.0;  // seconds: closer to the start than this restarts the video
		constexpr double      kEndGrace = 15.0;    // seconds: closer to the end than this counts as finished
		constexpr auto        kSaveInterval = std::chrono::seconds(5);

		struct Entry
		{
			std::string  key;
			std::string  source;
			std::string  title;
			double       position{ 0.0 };
			double       duration{ 0.0 };
			std::int64_t updated{ 0 };  // unix time
		};

		std::mutex                            g_mutex;
		std::vector<Entry>                    g_entries;
		bool                                  g_dirty{ false };
		std::chrono::steady_clock::time_point g_lastSave;

		std::optional<std::filesystem::path> FilePath()
		{
			auto directory = SKSE::log::log_directory();
			if (!directory) {
				return std::nullopt;
			}
			return *directory / "PiPPlayer_History.txt";
		}

		bool IsIdChar(char a_char)
		{
			return std::isalnum(static_cast<unsigned char>(a_char)) || a_char == '-' || a_char == '_';
		}

		// YouTube links differ by form (watch?v=, youtu.be/, shorts/, live/, embed/) and extra parameters; key them
		// by video id so every form resumes the same entry. Other sources are keyed by their exact text.
		std::string KeyFor(std::string_view a_source)
		{
			std::string lower(a_source);
			std::ranges::transform(lower, lower.begin(), [](unsigned char a_char) { return static_cast<char>(std::tolower(a_char)); });

			std::size_t idStart = std::string::npos;
			if (const auto at = lower.find("youtu.be/"); at != std::string::npos) {
				idStart = at + 9;
			} else if (lower.find("youtube.com/") != std::string::npos) {
				for (const auto marker : { "?v="sv, "&v="sv, "/shorts/"sv, "/live/"sv, "/embed/"sv }) {
					if (const auto at = lower.find(marker); at != std::string::npos) {
						idStart = at + marker.size();
						break;
					}
				}
			}
			if (idStart != std::string::npos) {
				std::size_t idEnd = idStart;
				while (idEnd < a_source.size() && IsIdChar(a_source[idEnd])) {
					++idEnd;
				}
				if (idEnd - idStart == 11) {
					return std::format("youtube:{}", a_source.substr(idStart, 11));
				}
			}
			return std::string(a_source);
		}

		double Resumable(double a_position, double a_duration)
		{
			return a_duration > 0.0 && a_position >= kStartGrace && a_position <= a_duration - kEndGrace ? a_position : 0.0;
		}

		std::int64_t Now()
		{
			return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
		}
	}

	void Load()
	{
		const auto path = FilePath();
		if (!path) {
			return;
		}
		std::ifstream file(*path, std::ios::binary);
		if (!file) {
			return;
		}

		std::vector<Entry> entries;
		std::string        line;
		while (std::getline(file, line) && entries.size() < kMaxEntries) {
			if (!line.empty() && line.back() == '\r') {
				line.pop_back();
			}
			// position <TAB> duration <TAB> unix time <TAB> title <TAB> source (older files have no title field)
			std::array<std::string_view, 4> fields;
			std::string_view                rest = line;
			bool                            valid = true;
			for (std::size_t i = 0; i < 3 && valid; ++i) {
				const auto tab = rest.find('\t');
				valid = tab != std::string_view::npos;
				if (valid) {
					fields[i] = rest.substr(0, tab);
					rest.remove_prefix(tab + 1);
				}
			}
			if (!valid || rest.empty()) {
				continue;
			}
			Entry entry;
			if (const auto tab = rest.find('\t'); tab != std::string_view::npos) {
				entry.title = std::string(rest.substr(0, tab));
				rest.remove_prefix(tab + 1);
			}
			entry.source = std::string(rest);
			entry.key = KeyFor(entry.source);
			std::from_chars(fields[0].data(), fields[0].data() + fields[0].size(), entry.position);
			std::from_chars(fields[1].data(), fields[1].data() + fields[1].size(), entry.duration);
			std::from_chars(fields[2].data(), fields[2].data() + fields[2].size(), entry.updated);
			entries.push_back(std::move(entry));
		}

		std::scoped_lock lock(g_mutex);
		g_entries = std::move(entries);
		logger::info("Loaded {} history entries", g_entries.size());
	}

	double ResumePosition(std::string_view a_source)
	{
		const auto       key = KeyFor(a_source);
		std::scoped_lock lock(g_mutex);
		const auto       entry = std::ranges::find(g_entries, key, &Entry::key);
		return entry != g_entries.end() ? entry->position : 0.0;
	}

	std::vector<Item> Recent()
	{
		std::scoped_lock  lock(g_mutex);
		std::vector<Item> items;
		items.reserve(g_entries.size());
		for (const auto& entry : g_entries) {
			items.push_back({ entry.source, entry.title, entry.position, entry.duration });
		}
		return items;
	}

	std::optional<std::string> LastSource()
	{
		std::scoped_lock lock(g_mutex);
		return g_entries.empty() ? std::nullopt : std::optional{ g_entries.front().source };
	}

	void Record(std::string_view a_source, double a_position, double a_duration, std::string_view a_title)
	{
		if (a_source.empty() || a_source.find_first_of("\t\r\n") != std::string_view::npos) {
			return;
		}
		auto             key = KeyFor(a_source);
		std::scoped_lock lock(g_mutex);

		auto entry = std::ranges::find(g_entries, key, &Entry::key);
		if (entry == g_entries.end()) {
			g_entries.insert(g_entries.begin(), Entry{ .key = std::move(key) });
			if (g_entries.size() > kMaxEntries) {
				g_entries.pop_back();
			}
			entry = g_entries.begin();
		} else if (entry != g_entries.begin()) {
			std::rotate(g_entries.begin(), entry, entry + 1);
			entry = g_entries.begin();
		}

		// One line per entry: tabs and line breaks in a title become spaces.
		std::string title(a_title);
		std::ranges::replace_if(title, [](char a_char) { return a_char == '\t' || a_char == '\r' || a_char == '\n'; }, ' ');
		if (!title.empty() && entry->title != title) {
			entry->title = std::move(title);
			g_dirty = true;
		}

		const double position = Resumable(a_position, a_duration);
		if (entry->source != a_source || std::abs(entry->position - position) >= 1.0 || entry->duration != a_duration) {
			entry->source = std::string(a_source);
			entry->position = position;
			entry->duration = a_duration;
			entry->updated = Now();
			g_dirty = true;
		}
	}

	void Save(bool a_force)
	{
		std::string contents;
		{
			std::scoped_lock lock(g_mutex);
			const auto       now = std::chrono::steady_clock::now();
			if (!g_dirty || (!a_force && now - g_lastSave < kSaveInterval)) {
				return;
			}
			for (const auto& entry : g_entries) {
				contents += std::format("{:.1f}\t{:.1f}\t{}\t{}\t{}\n", entry.position, entry.duration, entry.updated, entry.title, entry.source);
			}
			g_dirty = false;
			g_lastSave = now;
		}

		const auto path = FilePath();
		if (!path) {
			return;
		}
		auto temporary = *path;
		temporary += L".tmp";
		{
			std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
			file << contents;
			if (!file) {
				logger::error("Could not write {}", temporary.string());
				return;
			}
		}
		if (!::MoveFileExW(temporary.c_str(), path->c_str(), MOVEFILE_REPLACE_EXISTING)) {
			logger::error("Could not replace {} ({})", path->string(), ::GetLastError());
		}
	}
}
