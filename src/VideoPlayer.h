#pragma once

#include "Settings.h"

namespace PiP::VideoPlayer
{
	enum class State
	{
		Idle,
		Loading,
		Playing,
		Paused,
		Error
	};

	struct Status
	{
		State       state{ State::Idle };
		std::string message;       // shown while no video frame is on screen
		double      aspect{ 0.0 };  // display aspect ratio of the current video, 0 when unknown
		double      position{ 0.0 };  // seconds
		double      duration{ 0.0 };  // seconds, 0 when unknown (e.g. live streams)
		int              videoHeight{ 0 };  // height of the playing video track
		std::vector<int> qualities;         // heights the current video offers, best first; empty unless switchable
		bool             buffering{ false };  // playback is waiting for the network
		std::string      title;               // the YouTube title, or the file name
	};

	struct FrameView
	{
		int                 width{ 0 };  // 0 = no frame (nothing loaded)
		int                 height{ 0 };
		std::size_t         stride{ 0 };
		const std::uint8_t* pixels{ nullptr };  // B8G8R8A8, opaque
	};

	// Safe from any thread; playback runs on a worker thread that owns libmpv.
	void Play(std::string a_source);  // UTF-8 URL or file path
	void TogglePause();
	void SetPaused(bool a_paused);
	void Stop();
	void SeekBy(double a_seconds);
	void SeekTo(double a_fraction);  // 0..1 of the duration
	void SetVolume(int a_volume);    // 0..100
	// Switches the current video to the a_height track (0 = best) and makes it the default for YouTube videos.
	void SelectQuality(int a_height);
	// How the next YouTube lookups use QuickJS (see Settings::youTubeSolver).
	void SetYouTubeSolver(YouTubeSolver a_solver);
	void ShowNotice(std::string a_message);  // shown only while nothing is playing

	[[nodiscard]] Status GetStatus();

	// Render thread: when a frame newer than a_sequence exists, calls a_upload with it and advances a_sequence.
	void ConsumeFrame(std::uint64_t& a_sequence, const std::function<void(const FrameView&)>& a_upload);
}
