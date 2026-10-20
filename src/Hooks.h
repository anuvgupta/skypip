#pragma once

namespace PiP::Hooks
{
	// Hooks the renderer (device capture after BSGraphics::InitD3D, overlay drawing after Renderer::End) and the
	// game's input dispatch (hotkeys, control panel input).
	void Install();
}
