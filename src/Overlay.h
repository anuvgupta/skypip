#pragma once

namespace PiP::Overlay
{
	// Render thread entry points, called from the renderer hooks.
	void Initialize();
	void Render();

	// Safe from any thread.
	void SetVisible(bool a_visible);
	void ToggleVisible();

	// Input thread: queues a game input event for the control panel UI; applied on the next frame.
	void FeedInput(const RE::InputEvent& a_event);

	// UI scale relative to a 1080p screen.
	[[nodiscard]] float Scale();
}
