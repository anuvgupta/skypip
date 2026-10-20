#pragma once

namespace PiP::Input
{
	// Handles one batch of game input events (called from the input dispatch hook). Returns true when the game
	// must not see the batch: the control panel is open and takes keyboard and mouse.
	bool Process(RE::InputEvent* const* a_events);

	// Logs a warning for each hotkey that is also bound to a game keyboard control; call once data is loaded.
	void WarnAboutConflicts();
}
