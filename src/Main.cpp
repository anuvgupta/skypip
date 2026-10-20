#include "History.h"
#include "Hooks.h"
#include "Input.h"
#include "Overlay.h"
#include "Settings.h"
#include "VideoPlayer.h"

namespace
{
	void OnMessage(SKSE::MessagingInterface::Message* a_message)
	{
		switch (a_message->type) {
		case SKSE::MessagingInterface::kDataLoaded:
			PiP::Input::WarnAboutConflicts();
			break;
		case SKSE::MessagingInterface::kPostLoadGame:
		case SKSE::MessagingInterface::kNewGame:
			{
				// Resume the last video once, when play starts (not on the main menu).
				static bool resumed = false;
				if (resumed || !PiP::Settings::Get().resumeOnStart) {
					break;
				}
				resumed = true;
				if (auto last = PiP::History::LastSource()) {
					PiP::Overlay::SetVisible(true);
					PiP::VideoPlayer::Play(std::move(*last));
				}
			}
			break;
		default:
			break;
		}
	}
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	SKSE::Init(a_skse);
	logger::info("Game runtime {}", REL::Module::get().version().string("."sv));

	PiP::Settings::Load();
	PiP::History::Load();
	PiP::Hooks::Install();

	if (!SKSE::GetMessagingInterface()->RegisterListener(OnMessage)) {
		logger::error("Failed to register the SKSE message listener; hotkey conflict check skipped");
	}
	return true;
}
