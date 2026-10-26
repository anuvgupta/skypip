#pragma once

namespace PiP::AutoPause
{
	// Pauses the video while the player is in a conversation and resumes it afterwards (see the PauseForDialogue
	// settings). Register once game data is loaded; Update every frame.
	void Register();
	void Update();
}
