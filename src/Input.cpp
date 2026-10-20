#include "Input.h"

#include "Keys.h"
#include "Overlay.h"
#include "Panel.h"
#include "Settings.h"
#include "VideoPlayer.h"

namespace PiP::Input
{
	namespace
	{
		constexpr std::uint32_t kEscape = 0x01;

		std::string_view ContextName(std::size_t a_context)
		{
			constexpr std::string_view shared[]{ "gameplay", "menus", "console", "item menu", "inventory", "debug text",
				"favorites", "map", "stats", "cursor", "book", "debug overlay", "journal", "free camera", "map debug",
				"lockpicking" };
			if (a_context < std::size(shared)) {
				return shared[a_context];
			}
			// AE inserts the Creations menu context before Favor.
			return REL::Module::IsAE() && a_context == 16 ? "creations menu"sv : "favor"sv;
		}

		bool IsConsoleOpen()
		{
			auto* ui = RE::UI::GetSingleton();
			return ui && ui->IsMenuOpen(RE::Console::MENU_NAME);
		}

		// The key of a keyboard press event; nullopt for anything else, including the event types added in 1.7.99.
		std::optional<std::uint32_t> PressedKey(const RE::InputEvent& a_event)
		{
			if (a_event.GetEventType() != RE::INPUT_EVENT_TYPE::kButton || a_event.GetDevice() != RE::INPUT_DEVICE::kKeyboard) {
				return std::nullopt;
			}
			const auto* button = a_event.AsButtonEvent();
			if (!button || !button->IsDown()) {
				return std::nullopt;
			}
			return button->GetIDCode();
		}
	}

	bool Process(RE::InputEvent* const* a_events)
	{
		const bool panelWasOpen = Panel::IsOpen();
		if (!a_events) {
			return panelWasOpen;
		}

		const auto settings = Settings::Get();
		for (auto* event = *a_events; event; event = event->next) {
			const auto key = PressedKey(*event);

			if (Panel::IsOpen()) {
				if (key == settings.togglePanelKey || key == kEscape) {
					Panel::Close();
				} else {
					Overlay::FeedInput(*event);
				}
				continue;
			}

			if (!key || IsConsoleOpen()) {
				continue;
			}
			if (*key == settings.togglePanelKey) {
				Panel::Open();
			} else if (*key == settings.togglePauseKey) {
				VideoPlayer::TogglePause();
			} else if (*key == settings.toggleVisibleKey) {
				Overlay::ToggleVisible();
			}
		}
		// Also swallows the key that closed the panel, so Escape does not reach the game's menus.
		return panelWasOpen || Panel::IsOpen();
	}

	void WarnAboutConflicts()
	{
		const auto* controls = RE::ControlMap::GetSingleton();
		if (!controls) {
			logger::warn("Control map is missing; skipping the hotkey conflict check");
			return;
		}

		const auto                                        settings = Settings::Get();
		const std::pair<std::string_view, std::uint32_t> hotkeys[]{
			{ "TogglePanel", settings.togglePanelKey },
			{ "TogglePause", settings.togglePauseKey },
			{ "ToggleVisible", settings.toggleVisibleKey },
		};
		// SE has 17 input contexts; AE adds one (the controlMap array is that long).
		const std::size_t contexts = REL::Module::IsAE() ? 18 : 17;

		bool conflict = false;
		for (const auto& [action, key] : hotkeys) {
			for (std::size_t context = 0; context < contexts; ++context) {
				const auto* mappings = controls->controlMap[context];
				if (!mappings) {
					continue;
				}
				for (const auto& mapping : mappings->deviceMappings[RE::INPUT_DEVICE::kKeyboard]) {
					if (mapping.inputKey == key) {
						logger::warn("{} key {} is also the game's '{}' key ({} controls); change it in PiPPlayer.ini",
							action, Keys::KeyName(key), mapping.eventID.data(), ContextName(context));
						conflict = true;
					}
				}
			}
		}
		if (!conflict) {
			logger::info("Hotkeys do not overlap the game's keyboard controls");
		}
	}
}
