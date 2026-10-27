#pragma once

namespace PiP::History
{
	// Recently played videos and where playback left off, most recent first (at most 100), kept in
	// My Games/Skyrim Special Edition/SKSE/PiPPlayer_History.txt. Safe from any thread.
	void Load();

	struct Item
	{
		std::string source;
		std::string title;
		double      position{ 0.0 };  // where it resumes; 0 = from the start
		double      duration{ 0.0 };
	};

	// Most recent first.
	[[nodiscard]] std::vector<Item> Recent();

	// Where to resume a_source: 0 when unknown, near the start or end, or a live stream.
	[[nodiscard]] double                     ResumePosition(std::string_view a_source);
	[[nodiscard]] std::optional<std::string> LastSource();
	// The most recent video, if it was still playing or paused when the game last closed.
	[[nodiscard]] std::optional<std::string> OpenSource();

	// Whether the most recent video is still playing or paused, rather than stopped, finished, or failed.
	void SetOpen(bool a_open);

	// Updates a_source's entry in memory and moves it to the front; an empty a_title keeps the stored one.
	void Record(std::string_view a_source, double a_position, double a_duration, std::string_view a_title);

	// Writes changes to disk: at most every few seconds, or now when a_force.
	void Save(bool a_force);
}
