#include "AutoPause.h"

#include "Settings.h"
#include "VideoPlayer.h"

namespace PiP::AutoPause
{
	namespace
	{
		using Clock = std::chrono::steady_clock;

		// mpv applies a pause asynchronously; until it reports the new state, a playing status is not a user's resume.
		constexpr auto kPauseSettleTime = std::chrono::seconds(1);

		std::mutex        g_mutex;
		bool              g_inConversation{ false };
		bool              g_autoPaused{ false };  // the video is paused because of a conversation
		bool              g_pausePending{ false };  // a conversation started while a video was loading: pause it once it plays
		int               g_linesPlaying{ 0 };
		Clock::time_point g_pausedAt;
		Clock::time_point g_conversationEnded;
		Clock::time_point g_lastLine;  // a line started or ended, or the conversation opened

		// g_mutex held.
		void PauseForConversation()
		{
			if (g_autoPaused || !Settings::Get().pauseForDialogue) {
				return;
			}
			const auto state = VideoPlayer::GetStatus().state;
			if (state == VideoPlayer::State::Loading) {
				// Nothing plays yet; Update pauses the video as soon as it does, unless the conversation is over by then.
				if (!g_pausePending) {
					g_pausePending = true;
					logger::info("Will pause for a conversation once the video has loaded");
				}
				return;
			}
			if (state != VideoPlayer::State::Playing) {
				return;
			}
			g_pausePending = false;
			VideoPlayer::SetPaused(true);
			g_autoPaused = true;
			g_pausedAt = Clock::now();
			logger::info("Paused for a conversation");
		}

		class Listener final :
			public RE::BSTEventSink<RE::MenuOpenCloseEvent>,
			public RE::BSTEventSink<RE::TESTopicInfoEvent>
		{
		public:
			static Listener* Get()
			{
				static Listener listener;
				return &listener;
			}

			RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
			{
				if (a_event && a_event->menuName == RE::DialogueMenu::MENU_NAME) {
					std::scoped_lock lock(g_mutex);
					g_inConversation = a_event->opening;
					if (a_event->opening) {
						g_linesPlaying = 0;
						g_lastLine = Clock::now();
						PauseForConversation();
					} else {
						g_conversationEnded = Clock::now();
					}
				}
				return RE::BSEventNotifyControl::kContinue;
			}

			// Spoken lines: counted only during a conversation, to tell when it has gone quiet.
			RE::BSEventNotifyControl ProcessEvent(const RE::TESTopicInfoEvent* a_event, RE::BSTEventSource<RE::TESTopicInfoEvent>*) override
			{
				std::scoped_lock lock(g_mutex);
				if (!a_event || !g_inConversation) {
					return RE::BSEventNotifyControl::kContinue;
				}
				g_lastLine = Clock::now();
				if (a_event->type == RE::TESTopicInfoEvent::TopicInfoEventType::kTopicBegin) {
					++g_linesPlaying;
					PauseForConversation();  // talking again after a resume for silence
				} else {
					g_linesPlaying = std::max(0, g_linesPlaying - 1);
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};
	}

	void Register()
	{
		if (auto* ui = RE::UI::GetSingleton()) {
			ui->AddEventSink<RE::MenuOpenCloseEvent>(Listener::Get());
		}
		if (auto* events = RE::ScriptEventSourceHolder::GetSingleton()) {
			events->AddEventSink<RE::TESTopicInfoEvent>(Listener::Get());
		}
		logger::info("Conversation auto-pause registered");
	}

	void Update()
	{
		std::scoped_lock lock(g_mutex);
		if (!g_autoPaused && !g_pausePending) {
			return;
		}

		const auto now = Clock::now();
		const auto settings = Settings::Get();
		const bool conversationOver = !g_inConversation && settings.resumeAfterDialogue > 0 &&
		                              now - g_conversationEnded >= std::chrono::seconds(settings.resumeAfterDialogue);
		const bool quiet = settings.resumeAfterSilence > 0 && g_linesPlaying == 0 &&
		                   now - g_lastLine >= std::chrono::seconds(settings.resumeAfterSilence);
		const auto state = VideoPlayer::GetStatus().state;

		if (g_pausePending) {
			if (conversationOver || quiet) {
				g_pausePending = false;  // it may play once loaded
				logger::info("No longer pausing for the conversation: it {}", conversationOver ? "ended" : "went quiet");
			} else if (state != VideoPlayer::State::Loading) {
				g_pausePending = false;
				if (state == VideoPlayer::State::Playing) {
					VideoPlayer::SetPaused(true);
					g_autoPaused = true;
					g_pausedAt = now;
					logger::info("Paused for a conversation once the video had loaded");
				}
			}
			return;
		}

		if (state != VideoPlayer::State::Paused) {
			if (now - g_pausedAt > kPauseSettleTime) {
				g_autoPaused = false;  // resumed by the player, or a different video started
			}
			return;
		}

		if (conversationOver || quiet) {
			VideoPlayer::SetPaused(false);
			g_autoPaused = false;
			logger::info("Resumed after a conversation ({})", conversationOver ? "it ended" : "no one spoke");
		}
	}
}
