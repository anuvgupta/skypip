#pragma once

#include <imgui.h>

namespace PiP::Panel
{
	// The control panel: opened with the TogglePanel key; freezes the game and takes keyboard and mouse input.
	// Safe from any thread.
	void               Open();
	void               Close();  // also saves changed settings
	[[nodiscard]] bool IsOpen();

	// Render thread, between ImGui::NewFrame and ImGui::Render: the panel beside the video and the controls on it.
	void Draw(ImVec2 a_videoMin, ImVec2 a_videoMax, bool a_hasFrame);

	// Render thread: how far the video is being dragged from its configured position.
	[[nodiscard]] ImVec2 DragOffset();
}
