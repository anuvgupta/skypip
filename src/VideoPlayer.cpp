#include "VideoPlayer.h"

#include <charconv>

#include "History.h"
#include "Mpv.h"
#include "Paths.h"
#include "Settings.h"
#include "YouTube.h"

namespace PiP::VideoPlayer
{
	namespace
	{
		// mpv's software renderer wants 64-byte aligned rows and buffers.
		class FrameBuffer
		{
		public:
			void Resize(int a_width, int a_height)
			{
				width = a_width;
				height = a_height;
				stride = (static_cast<std::size_t>(a_width) * 4 + 63) & ~std::size_t{ 63 };
				const auto size = stride * static_cast<std::size_t>(a_height);
				if (size > _capacity) {
					_storage.reset(static_cast<std::uint8_t*>(::_aligned_malloc(size, 64)));
					_capacity = _storage ? size : 0;
				}
			}

			[[nodiscard]] std::uint8_t* data() const noexcept { return _storage.get(); }
			[[nodiscard]] bool          valid() const noexcept { return width > 0 && _storage; }

			int         width{ 0 };
			int         height{ 0 };
			std::size_t stride{ 0 };

		private:
			struct AlignedFree
			{
				void operator()(std::uint8_t* a_ptr) const noexcept { ::_aligned_free(a_ptr); }
			};

			std::unique_ptr<std::uint8_t, AlignedFree> _storage;
			std::size_t                                _capacity{ 0 };
		};

		// Wine (CrossOver, Proton) has no D3D11/DXVA2 video decoding; probing for it only fails noisily.
		bool RunningUnderWine()
		{
			const HMODULE ntdll = ::GetModuleHandleW(L"ntdll.dll");
			return ntdll && ::GetProcAddress(ntdll, "wine_get_version");
		}

		std::string_view EndReasonName(mpv_end_file_reason a_reason)
		{
			switch (a_reason) {
			case MPV_END_FILE_REASON_EOF:
				return "end of file"sv;
			case MPV_END_FILE_REASON_STOP:
				return "stopped"sv;
			case MPV_END_FILE_REASON_QUIT:
				return "quit"sv;
			case MPV_END_FILE_REASON_ERROR:
				return "error"sv;
			case MPV_END_FILE_REASON_REDIRECT:
				return "redirect"sv;
			default:
				return "unknown"sv;
			}
		}

		// YouTube stream links expire after about six hours; switching tracks after that needs a fresh lookup.
		constexpr auto kStreamLinkLifetime = std::chrono::hours(5);
		// A stream that fails again this soon after RecoverStream reloaded it is left alone rather than reloaded in a loop.
		constexpr auto kRecoveryInterval = std::chrono::seconds(60);

		// The ytdl hook lists every YouTube format as an edition titled like
		// "298 - 1280x720 (720p60) (h264 60 fps 1897 kbps)"; switching editions only switches streams.
		struct Edition
		{
			std::int64_t id{ 0 };
			int          height{ 0 };
			int          codecRank{ 3 };  // h264, vp9, av1, other: cheapest to decode first
			bool         labeled{ false };  // has YouTube's "(720p)" label; unlabeled ones are HLS variants
			int          kbps{ 0 };
		};

		std::optional<Edition> ParseEdition(std::int64_t a_id, std::string_view a_title)
		{
			Edition edition{ .id = a_id };
			// Height: the number after "<width>x".
			for (std::size_t x = a_title.find('x'); x != std::string_view::npos; x = a_title.find('x', x + 1)) {
				if (x == 0 || !std::isdigit(static_cast<unsigned char>(a_title[x - 1]))) {
					continue;
				}
				int height = 0;
				const auto [end, error] = std::from_chars(a_title.data() + x + 1, a_title.data() + a_title.size(), height);
				if (error == std::errc{} && height > 0) {
					edition.height = height;
					break;
				}
			}
			if (edition.height == 0) {
				return std::nullopt;
			}
			constexpr std::pair<std::string_view, int> codecs[]{ { "(h264", 0 }, { "(vp9", 1 }, { "(av1", 2 } };
			for (const auto& [codec, rank] : codecs) {
				if (a_title.find(codec) != std::string_view::npos) {
					edition.codecRank = rank;
					break;
				}
			}
			edition.labeled = a_title.find(std::format("({}p", edition.height)) != std::string_view::npos;
			if (const auto kbps = a_title.rfind(" kbps"); kbps != std::string_view::npos) {
				auto start = a_title.find_last_not_of("0123456789", kbps - 1);
				start = start == std::string_view::npos ? 0 : start + 1;
				std::from_chars(a_title.data() + start, a_title.data() + kbps, edition.kbps);
			}
			return edition;
		}

		std::string_view NodeString(const mpv_node& a_node)
		{
			return a_node.format == MPV_FORMAT_STRING && a_node.u.string ? std::string_view(a_node.u.string) : std::string_view{};
		}

		// yt-dlp options with and without QuickJS, which solves YouTube's JavaScript challenge. Many lookups work without
		// it, which saves starting QuickJS (about 1-2 s under CrossOver); see Settings::youTubeSolver.
		constexpr auto kYtdlOptionsWithoutJs = "no-config=,no-js-runtimes=";
		constexpr auto kYtdlOptionsWithJs = "no-config=,js-runtimes=quickjs";

		// yt-dlp's reason from an "ERROR: [youtube] <id>: <reason>. <advice>" line, cut to its first sentence; empty for
		// any other line, such as the hook's generic "youtube-dl failed: unexpected error occurred" that follows it.
		std::string_view YtdlReason(std::string_view a_line)
		{
			constexpr auto prefix = "ERROR: "sv;
			if (!a_line.starts_with(prefix)) {
				return {};
			}
			a_line.remove_prefix(prefix.size());
			if (a_line.starts_with('[')) {
				if (const auto colon = a_line.find(": "); colon != std::string_view::npos) {
					a_line.remove_prefix(colon + 2);
				}
			}
			if (const auto end = a_line.find(". "); end != std::string_view::npos) {
				a_line = a_line.substr(0, end + 1);
			}
			return a_line;
		}

		// What the player is doing before mpv reports the file loaded.
		std::string OpeningMessage(std::string_view a_source)
		{
			if (YouTube::Available() && YouTube::IsYouTubeLink(a_source)) {
				return "Looking up video on YouTube...";
			}
			const bool web = a_source.starts_with("http://") || a_source.starts_with("https://");
			return web ? "Opening link..." : "Opening file...";
		}

		enum PropertyId : std::uint64_t
		{
			kDisplayWidth = 1,
			kDisplayHeight,
			kPause,
			kPosition,
			kDuration,
			kEditions,
			kCurrentEdition,
			kPath,
			kTitle,
			kBuffering,
			kBufferPercent,
		};

		class Player
		{
		public:
			// Never destroyed: the worker thread runs until the game exits.
			static Player& Get()
			{
				static auto* player = new Player();
				return *player;
			}

			void Post(std::function<void()> a_command)
			{
				{
					std::scoped_lock lock(_queueMutex);
					_commands.push_back(std::move(a_command));
				}
				std::call_once(_started, [this] { std::thread(&Player::Run, this).detach(); });
				Wake();
			}

			void Play(std::string a_source)
			{
				if (YouTube::IsYouTubeLink(a_source) && !YouTube::Available()) {
					logger::warn("{} is missing; cannot play {}", Paths::ToUtf8(YouTube::Executable()), a_source);
					SetStatus(State::Error,
						"YouTube links need the YouTube tools (yt-dlp.exe and qjs.exe) in Data/SKSE/Plugins/PiPPlayer. "
						"See the mod page for downloads.");
					return;
				}
				SetStatus(State::Loading, OpeningMessage(a_source));
				Post([this, source = std::move(a_source)] {
					if (!_mpv) {
						SetStatus(State::Error, _loadError);
						return;
					}
					const double resume = History::ResumePosition(source);
					logger::info("Playing {}{}", source, resume > 0.0 ? std::format(" from {:.0f} s", resume) : std::string{});
					FadeOut();
					LoadFile(source, resume);
				});
			}

			void TogglePause() { Command({ "cycle", "pause" }); }
			void SetPaused(bool a_paused) { Command({ "set", "pause", a_paused ? "yes" : "no" }); }
			void Stop()
			{
				Post([this] {
					if (_mpv) {
						FadeOut();
						RunCommand({ "stop" });
						RunCommand({ "set", "volume", std::to_string(_volume) });
					}
				});
			}
			void Reload()
			{
				Post([this] {
					if (!_mpv || _requestedSource.empty()) {
						return;
					}
					double start = 0.0;
					if (_position > 0.0) {
						const bool atEnd = _duration > 0.0 && _position >= _duration - 1.0;
						start = atEnd ? 0.0 : _position;
					} else {
						start = History::ResumePosition(_requestedSource);  // stopped: where it was left
					}
					logger::info("Reloading {} from {:.0f} s", _requestedSource, start);
					const auto source = _requestedSource;
					SetStatus(State::Loading, OpeningMessage(source));
					FadeOut();
					RunCommand({ "set", "pause", "no" });
					LoadFile(source, start);
				});
			}
			void SeekBy(double a_seconds) { Command({ "seek", std::format("{:.3f}", a_seconds), "relative" }); }
			void SeekTo(double a_fraction) { Command({ "seek", std::format("{:.3f}", std::clamp(a_fraction, 0.0, 1.0) * 100.0), "absolute-percent" }); }
			void SetVolume(int a_volume)
			{
				Post([this, volume = std::clamp(a_volume, 0, 100)] {
					_volume = volume;
					if (_mpv && !_fadeInPending) {
						RunCommand({ "set", "volume", std::to_string(volume) });
					}
				});
			}

			void SetYouTubeSolver(YouTubeSolver a_solver)
			{
				Post([this, a_solver] {
					_solver = a_solver;
					if (_mpv && !_retryingWithQuickJs) {
						ApplySolverOptions();
					}
				});
			}

			void SelectQuality(int a_height)
			{
				Post([this, a_height] {
					if (!_mpv) {
						return;
					}
					RunCommand({ "set", "ytdl-format", YouTube::FormatFor(a_height) });

					const auto* edition = PickEdition(a_height);
					if (!edition || edition->id == _currentEdition) {
						return;
					}
					if (std::chrono::steady_clock::now() - _loadedAt > kStreamLinkLifetime) {
						logger::info("Reloading {} for {}p (stream links expired)", _currentPath, edition->height);
						FadeOut();
						LoadFile(_currentPath, _position);
					} else {
						logger::info("Switching to {}p (edition {})", edition->height, edition->id);
						RunCommand({ "set", "edition", std::to_string(edition->id) });
					}
				});
			}

			// Runs an mpv command asynchronously on the worker thread; ignored until libmpv is loaded.
			void Command(std::vector<std::string> a_args)
			{
				Post([this, args = std::move(a_args)] {
					if (_mpv) {
						RunCommand(args);
					}
				});
			}

			void ShowNotice(std::string a_message)
			{
				std::scoped_lock lock(_statusMutex);
				if (_status.state != State::Playing && _status.state != State::Paused && _status.state != State::Loading) {
					_status.state = State::Idle;
					_status.message = std::move(a_message);
				}
			}

			Status GetStatus()
			{
				std::scoped_lock lock(_statusMutex);
				return _status;
			}

			void ConsumeFrame(std::uint64_t& a_sequence, const std::function<void(const FrameView&)>& a_upload)
			{
				std::scoped_lock lock(_frameMutex);
				if (a_sequence == _frameSequence) {
					return;
				}
				a_sequence = _frameSequence;
				a_upload(FrameView{ _front.width, _front.height, _front.stride, _front.data() });
			}

		private:
			static void OnWakeup(void* a_self) { static_cast<Player*>(a_self)->Wake(); }

			void Wake()
			{
				{
					std::scoped_lock lock(_queueMutex);
					_wake = true;
				}
				_wakeCondition.notify_one();
			}

			void Run()
			{
				Initialize();

				for (;;) {
					std::deque<std::function<void()>> commands;
					{
						std::unique_lock lock(_queueMutex);
						_wakeCondition.wait(lock, [this] { return _wake || !_commands.empty(); });
						_wake = false;
						commands.swap(_commands);
					}

					for (auto& command : commands) {
						command();
					}
					if (!_mpv) {
						continue;
					}

					DrainEvents();
					if (_api->mpv_render_context_update(_render) & MPV_RENDER_UPDATE_FRAME) {
						RenderFrame();
					}
				}
			}

			void Initialize()
			{
				_api = Mpv::Load(_loadError);
				if (!_api) {
					logger::error("{}", _loadError);
					return;
				}

				_mpv = _api->mpv_create();
				if (!_mpv) {
					Fail("mpv_create failed");
					return;
				}

				const auto& settings = Settings::Get();
				SetOption("vo", "libmpv");
				const bool wine = RunningUnderWine();
				SetOption("hwdec", wine ? "no" : "auto-copy");
				SetOption("video-timing-offset", "0");
				SetOption("keep-open", "yes");
				SetOption("osd-level", "0");
				SetOption("audio-client-name", "SkyPiP");
				SetOption("volume", std::to_string(settings.volume).c_str());
				_volume = settings.volume;
				// Opening and closing the audio device pops. Keep one device open in one format for every video, and
				// play silence through pauses and seeks instead of stopping it.
				SetOption("gapless-audio", "yes");
				SetOption("audio-samplerate", "48000");
				SetOption("audio-channels", "stereo");
				SetOption("audio-stream-silence", "yes");
				SetOption("loop-file", settings.loop ? "inf" : "no");
				// YouTube's servers drop long-lived connections. mpv's curl backend otherwise asks for each stream in one
				// open-ended request and gives up after 5 drops, which ends the video track while the audio plays on (a
				// frozen picture). Ask in 10 MiB pieces, as yt-dlp's own downloads do, and retry more before giving up.
				SetOption("curl-max-request-size", "10MiB");
				SetOption("curl-max-retries", "20");
				if (YouTube::Available()) {
					// QuickJS (qjs.exe) sits next to yt-dlp.exe, where yt-dlp finds it when a lookup needs it.
					SetOption("ytdl", "yes");
					SetOption("script-opts", std::format("ytdl_hook-ytdl_path={}", Paths::ToUtf8(YouTube::Executable())).c_str());
					_solver = settings.youTubeSolver;
					SetOption("ytdl-raw-options", _solver == YouTubeSolver::Always ? kYtdlOptionsWithJs : kYtdlOptionsWithoutJs);
					SetOption("ytdl-format", YouTube::FormatFor(settings.youTubeMaxHeight).c_str());
				} else {
					logger::warn("{} is missing; YouTube links will not play", Paths::ToUtf8(YouTube::Executable()));
					SetOption("ytdl", "no");
				}

				if (const auto error = _api->mpv_initialize(_mpv); error < 0) {
					Fail(std::format("mpv_initialize failed: {}", _api->mpv_error_string(error)));
					return;
				}

				mpv_render_param params[]{
					{ MPV_RENDER_PARAM_API_TYPE, const_cast<char*>(MPV_RENDER_API_TYPE_SW) },
					{ MPV_RENDER_PARAM_INVALID, nullptr },
				};
				if (const auto error = _api->mpv_render_context_create(&_render, _mpv, params); error < 0) {
					Fail(std::format("mpv_render_context_create failed: {}", _api->mpv_error_string(error)));
					return;
				}

				_api->mpv_request_log_messages(_mpv, "info");
				_api->mpv_observe_property(_mpv, kDisplayWidth, "dwidth", MPV_FORMAT_INT64);
				_api->mpv_observe_property(_mpv, kDisplayHeight, "dheight", MPV_FORMAT_INT64);
				_api->mpv_observe_property(_mpv, kPause, "pause", MPV_FORMAT_FLAG);
				_api->mpv_observe_property(_mpv, kPosition, "time-pos", MPV_FORMAT_DOUBLE);
				_api->mpv_observe_property(_mpv, kDuration, "duration", MPV_FORMAT_DOUBLE);
				_api->mpv_observe_property(_mpv, kEditions, "edition-list", MPV_FORMAT_NODE);
				_api->mpv_observe_property(_mpv, kCurrentEdition, "current-edition", MPV_FORMAT_INT64);
				_api->mpv_observe_property(_mpv, kPath, "path", MPV_FORMAT_STRING);
				_api->mpv_observe_property(_mpv, kTitle, "media-title", MPV_FORMAT_STRING);
				_api->mpv_observe_property(_mpv, kBuffering, "paused-for-cache", MPV_FORMAT_FLAG);
				_api->mpv_observe_property(_mpv, kBufferPercent, "cache-buffering-state", MPV_FORMAT_INT64);
				_api->mpv_set_wakeup_callback(_mpv, OnWakeup, this);
				_api->mpv_render_context_set_update_callback(_render, OnWakeup, this);

				logger::info("libmpv initialized (hardware decoding {})", wine ? "off under Wine" : "auto-copy");
			}

			// Worker thread only: volume ramps so cutting or starting audio does not click. Many small steps: each
			// volume change is itself a tiny jump.
			void RampVolume(int a_from, int a_to)
			{
				constexpr int  kSteps = 25;
				constexpr auto kStepTime = std::chrono::milliseconds(10);
				for (int step = 1; step <= kSteps; ++step) {
					RunCommand({ "set", "volume", std::format("{:.1f}", a_from + (a_to - a_from) * static_cast<double>(step) / kSteps) });
					std::this_thread::sleep_for(kStepTime);
				}
			}

			// Fades the playing audio out, then waits for the audio already queued for the device (mpv's 0.2 s
			// audio buffer, at the old volume) to play, so replacing or stopping the file cuts silence.
			void FadeOut()
			{
				if (!_audioPlaying || _paused || _fadeInPending) {
					return;
				}
				RampVolume(_volume, 0);
				std::this_thread::sleep_for(std::chrono::milliseconds(250));
				_audioPlaying = false;
			}

			// Sets yt-dlp's options for the next lookup: QuickJS when the setting says always, or while retrying.
			void ApplySolverOptions()
			{
				const bool withJs = _solver == YouTubeSolver::Always || _retryingWithQuickJs;
				RunCommand({ "set", "ytdl-raw-options", withJs ? kYtdlOptionsWithJs : kYtdlOptionsWithoutJs });
			}

			void EndQuickJsRetry()
			{
				if (_retryingWithQuickJs) {
					_retryingWithQuickJs = false;
					ApplySolverOptions();
				}
			}

			// Worker thread only.
			void RunCommand(const std::vector<std::string>& a_args)
			{
				std::vector<const char*> argv;
				for (const auto& arg : a_args) {
					argv.push_back(arg.c_str());
				}
				argv.push_back(nullptr);
				Check(_api->mpv_command_async(_mpv, 0, argv.data()), a_args.front());
			}

			void LoadFile(const std::string& a_source, double a_start)
			{
				_requestedSource = a_source;
				{
					std::scoped_lock lock(_statusMutex);
					_status.source = a_source;
				}
				if (a_start > 0.0) {
					RunCommand({ "loadfile", a_source, "replace", "-1", std::format("start={:.3f}", a_start) });
				} else {
					RunCommand({ "loadfile", a_source, "replace" });
				}
			}

			// The a_height edition (0 = the tallest): YouTube's labeled formats first, then the codec cheapest to decode,
			// then the lowest bitrate.
			const Edition* PickEdition(int a_height) const
			{
				int target = a_height;
				if (target <= 0) {
					for (const auto& edition : _editions) {
						target = std::max(target, edition.height);
					}
				}
				const Edition* best = nullptr;
				const auto     key = [](const Edition& a_edition) { return std::tuple(!a_edition.labeled, a_edition.codecRank, a_edition.kbps); };
				for (const auto& edition : _editions) {
					if (edition.height == target && (!best || key(edition) < key(*best))) {
						best = &edition;
					}
				}
				return best;
			}

			void OnEditionList(const mpv_node& a_list)
			{
				_editions.clear();
				if (a_list.format == MPV_FORMAT_NODE_ARRAY && a_list.u.list) {
					for (int i = 0; i < a_list.u.list->num; ++i) {
						const auto& entry = a_list.u.list->values[i];
						if (entry.format != MPV_FORMAT_NODE_MAP || !entry.u.list) {
							continue;
						}
						std::int64_t     id = -1;
						std::string_view title;
						for (int k = 0; k < entry.u.list->num; ++k) {
							const std::string_view key = entry.u.list->keys[k];
							const auto&            value = entry.u.list->values[k];
							if (key == "id" && value.format == MPV_FORMAT_INT64) {
								id = value.u.int64;
							} else if (key == "title") {
								title = NodeString(value);
							}
						}
						if (auto edition = ParseEdition(id, title); id >= 0 && edition) {
							_editions.push_back(*edition);
						}
					}
				}
				PublishQualities();
			}

			void PublishQualities()
			{
				std::vector<int> heights;
				int              current = 0;
				for (const auto& edition : _editions) {
					heights.push_back(edition.height);
					if (edition.id == _currentEdition) {
						current = edition.height;
					}
				}
				std::ranges::sort(heights, std::greater{});
				heights.erase(std::ranges::unique(heights).begin(), heights.end());
				if (heights.size() < 2) {
					heights.clear();
				}

				std::scoped_lock lock(_statusMutex);
				_status.videoHeight = current;
				_status.qualities = std::move(heights);
			}

			void Fail(std::string a_error)
			{
				logger::error("{}", a_error);
				_loadError = std::move(a_error);
				if (_render) {
					_api->mpv_render_context_free(_render);
					_render = nullptr;
				}
				if (_mpv) {
					_api->mpv_terminate_destroy(_mpv);
					_mpv = nullptr;
				}
			}

			void SetOption(const char* a_name, const char* a_value)
			{
				if (const auto error = _api->mpv_set_option_string(_mpv, a_name, a_value); error < 0) {
					logger::warn("mpv option {}={} rejected: {}", a_name, a_value, _api->mpv_error_string(error));
				}
			}

			void Check(int a_error, std::string_view a_what)
			{
				if (a_error < 0) {
					logger::error("mpv {} failed: {}", a_what, _api->mpv_error_string(a_error));
					SetStatus(State::Error, std::format("Could not play: {}", _api->mpv_error_string(a_error)));
				}
			}

			void DrainEvents()
			{
				for (;;) {
					const auto* event = _api->mpv_wait_event(_mpv, 0);
					switch (event->event_id) {
					case MPV_EVENT_NONE:
						return;
					case MPV_EVENT_START_FILE:
						// Silent until playback starts, then FadeIn (MPV_EVENT_PLAYBACK_RESTART).
						RunCommand({ "set", "volume", "0" });
						_fadeInPending = true;
						_ytdlError.clear();
						// Keep the display size: mpv only reports dwidth/dheight when they change, so a next file
						// of the same size sends no update.
						logger::info("mpv: opening");
						ClearFrame();
						_awaitingFrame = true;
						if (GetStatus().state != State::Loading) {  // Play() already said what is being opened
							SetStatus(State::Loading, "Opening...");
						}
						break;
					case MPV_EVENT_PLAYBACK_RESTART:
						_audioPlaying = true;
						if (_fadeInPending) {
							_fadeInPending = false;
							RampVolume(0, _volume);
						}
						break;
					case MPV_EVENT_FILE_LOADED:
						EndQuickJsRetry();
						logger::info("mpv: loaded");
						_loadedAt = std::chrono::steady_clock::now();
						// The first picture still needs the stream connected, buffered, and decoded.
						SetStatus(_paused ? State::Paused : State::Playing, "Buffering...");
						break;
					case MPV_EVENT_END_FILE:
						OnEndFile(*static_cast<const mpv_event_end_file*>(event->data));
						break;
					case MPV_EVENT_PROPERTY_CHANGE:
						OnPropertyChange(event->reply_userdata, *static_cast<const mpv_event_property*>(event->data));
						break;
					case MPV_EVENT_LOG_MESSAGE:
						{
							const auto& message = *static_cast<const mpv_event_log_message*>(event->data);
							std::string_view text = message.text;
							while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) {
								text.remove_suffix(1);
							}
							const auto level = message.log_level <= MPV_LOG_LEVEL_ERROR ? spdlog::level::err :
							                   message.log_level == MPV_LOG_LEVEL_WARN  ? spdlog::level::warn :
							                                                              spdlog::level::info;
							spdlog::log(level, "mpv [{}] {}", message.prefix, text);
							// The ytdl hook explains a failed YouTube load only in its log; keep yt-dlp's own reason for the
							// status, falling back to the hook's line when yt-dlp gave none.
							if (message.log_level <= MPV_LOG_LEVEL_ERROR && std::string_view(message.prefix) == "ytdl_hook"sv) {
								if (const auto reason = YtdlReason(text); !reason.empty()) {
									_ytdlError = reason;
								} else if (_ytdlError.empty()) {
									_ytdlError = text;
								}
							}
							if (message.log_level <= MPV_LOG_LEVEL_ERROR && std::string_view(message.prefix) == "curl"sv &&
								text.starts_with("transfer failed"sv)) {
								RecoverStream();
							}
						}
						break;
					default:
						break;
					}
				}
			}

			// mpv gave up on one of a playing file's streams, which ends that track while the other plays on: the picture
			// freezes under running audio, or the audio stops under a moving picture. Load the file again where it is, as
			// the Reload button does, unless the last recovery was too recent to have helped.
			void RecoverStream()
			{
				const auto state = GetStatus().state;
				if (state != State::Playing && state != State::Paused) {
					return;  // still opening: a failed open ends the file and reports its error
				}
				const auto now = std::chrono::steady_clock::now();
				if (_recoveredAt && now - *_recoveredAt < kRecoveryInterval) {
					logger::warn("Stream failed again within {} s of reloading; not reloading", kRecoveryInterval.count());
					return;
				}
				_recoveredAt = now;
				logger::info("Stream transfer failed; reloading");
				Reload();
			}

			void OnEndFile(const mpv_event_end_file& a_endFile)
			{
				_audioPlaying = false;
				logger::info("mpv: ended ({})", EndReasonName(a_endFile.reason));
				if (a_endFile.reason == MPV_END_FILE_REASON_EOF) {
					History::Record(_currentPath, _duration, _duration, _title);  // finished: next time starts over
				}
				History::Save(true);
				if (a_endFile.reason == MPV_END_FILE_REASON_ERROR && !_ytdlError.empty() && _solver == YouTubeSolver::Fallback &&
					!_retryingWithQuickJs && YouTube::IsYouTubeLink(_requestedSource)) {
					logger::info("Lookup without QuickJS failed ({}); retrying with QuickJS", _ytdlError);
					_retryingWithQuickJs = true;
					ApplySolverOptions();
					SetStatus(State::Loading, OpeningMessage(_requestedSource));
					LoadFile(_requestedSource, History::ResumePosition(_requestedSource));
					return;
				}
				EndQuickJsRetry();
				if (a_endFile.reason == MPV_END_FILE_REASON_ERROR) {
					const auto* reason = _api->mpv_error_string(a_endFile.error);
					logger::error("Playback failed: {}", reason);
					ClearFrame();
					SetStatus(State::Error, _ytdlError.empty() ? std::format("Could not play: {}", reason) : std::format("YouTube: {}", _ytdlError));
					History::SetOpen(false);
					History::Save(true);
				} else if (a_endFile.reason != MPV_END_FILE_REASON_REDIRECT && GetStatus().state != State::Loading) {
					ClearFrame();
					SetStatus(State::Idle, "");
					History::SetOpen(false);  // stopped or finished: nothing to resume next time
					History::Save(true);
				}
			}

			void OnPropertyChange(std::uint64_t a_id, const mpv_event_property& a_property)
			{
				if (a_id == kPosition || a_id == kDuration) {
					const double value = a_property.format == MPV_FORMAT_DOUBLE ? *static_cast<const double*>(a_property.data) : 0.0;
					(a_id == kPosition ? _position : _duration) = value;
					{
						std::scoped_lock lock(_statusMutex);
						(a_id == kPosition ? _status.position : _status.duration) = value;
					}
					if (a_id == kPosition && value > 0.0) {
						History::Record(_currentPath, _position, _duration, _title);
						History::SetOpen(true);
						History::Save(false);
					}
					return;
				}
				if (a_id == kEditions) {
					OnEditionList(a_property.format == MPV_FORMAT_NODE ? *static_cast<const mpv_node*>(a_property.data) : mpv_node{});
					return;
				}
				if (a_id == kCurrentEdition) {
					_currentEdition = a_property.format == MPV_FORMAT_INT64 ? *static_cast<const std::int64_t*>(a_property.data) : -1;
					PublishQualities();
					return;
				}
				if (a_id == kPath) {
					History::Save(true);  // the previous file's last position
					_currentPath = a_property.format == MPV_FORMAT_STRING ? *static_cast<char* const*>(a_property.data) : "";
					_duration = 0.0;
					// _title is kept: mpv sends media-title for the new file separately, possibly before the path.
					return;
				}
				if (a_id == kBuffering) {
					std::scoped_lock lock(_statusMutex);
					_status.buffering = a_property.format == MPV_FORMAT_FLAG && *static_cast<const int*>(a_property.data) != 0;
					return;
				}
				if (a_id == kBufferPercent) {
					if (_awaitingFrame && a_property.format == MPV_FORMAT_INT64) {
						std::scoped_lock lock(_statusMutex);
						if (_status.state == State::Playing || _status.state == State::Paused) {
							_status.message = std::format("Buffering... {}%", *static_cast<const std::int64_t*>(a_property.data));
						}
					}
					return;
				}
				if (a_id == kTitle) {
					_title = a_property.format == MPV_FORMAT_STRING ? *static_cast<char* const*>(a_property.data) : "";
					std::scoped_lock lock(_statusMutex);
					_status.title = _title;
					return;
				}
				if (a_property.format == MPV_FORMAT_NONE || !a_property.data) {
					return;
				}
				switch (a_id) {
				case kDisplayWidth:
					_videoWidth = static_cast<int>(*static_cast<const std::int64_t*>(a_property.data));
					break;
				case kDisplayHeight:
					_videoHeight = static_cast<int>(*static_cast<const std::int64_t*>(a_property.data));
					break;
				case kPause:
					{
						_paused = *static_cast<const int*>(a_property.data) != 0;
						History::Save(true);
						std::scoped_lock lock(_statusMutex);
						if (_status.state == State::Playing || _status.state == State::Paused) {
							_status.state = _paused ? State::Paused : State::Playing;
						}
					}
					return;
				default:
					return;
				}

				std::scoped_lock lock(_statusMutex);
				_status.aspect = _videoWidth > 0 && _videoHeight > 0 ? static_cast<double>(_videoWidth) / _videoHeight : 0.0;
			}

			void RenderFrame()
			{
				if (_videoWidth <= 0 || _videoHeight <= 0) {
					return;
				}

				const auto settings = Settings::Get();
				const int  width = settings.width;
				if (settings.brightness != _lutBrightness) {
					BuildBrightnessLut(settings.brightness);
				}
				const int height = std::max(2, static_cast<int>(std::lround(static_cast<double>(width) * _videoHeight / _videoWidth)));
				_back.Resize(width, height);
				if (!_back.valid()) {
					logger::error("Out of memory for a {}x{} frame", width, height);
					return;
				}

				int         size[2]{ width, height };
				std::size_t stride = _back.stride;
				char        format[] = "bgr0";
				mpv_render_param params[]{
					{ MPV_RENDER_PARAM_SW_SIZE, size },
					{ MPV_RENDER_PARAM_SW_FORMAT, format },
					{ MPV_RENDER_PARAM_SW_STRIDE, &stride },
					{ MPV_RENDER_PARAM_SW_POINTER, _back.data() },
					{ MPV_RENDER_PARAM_INVALID, nullptr },
				};
				if (const auto error = _api->mpv_render_context_render(_render, params); error < 0) {
					logger::error("mpv_render_context_render failed: {}", _api->mpv_error_string(error));
					return;
				}

				// "bgr0" leaves the fourth byte undefined; the overlay samples it as alpha. Brightness is applied in the
				// same pass.
				const bool adjust = _lutBrightness != 0;
				for (int y = 0; y < height; ++y) {
					auto* row = _back.data() + y * _back.stride;
					for (int x = 0; x < width; ++x) {
						auto* pixel = row + x * 4;
						if (adjust) {
							pixel[0] = _brightnessLut[pixel[0]];
							pixel[1] = _brightnessLut[pixel[1]];
							pixel[2] = _brightnessLut[pixel[2]];
						}
						pixel[3] = 0xFF;
					}
				}

				_awaitingFrame = false;
				std::scoped_lock lock(_frameMutex);
				std::swap(_front, _back);
				++_frameSequence;
			}

			// A gamma curve: brightens or darkens midtones while black stays black and white stays white.
			void BuildBrightnessLut(int a_brightness)
			{
				const double exponent = std::pow(2.0, -a_brightness / 50.0);
				for (int value = 0; value < 256; ++value) {
					_brightnessLut[value] = static_cast<std::uint8_t>(std::lround(255.0 * std::pow(value / 255.0, exponent)));
				}
				_lutBrightness = a_brightness;
			}

			void ClearFrame()
			{
				std::scoped_lock lock(_frameMutex);
				_front.width = 0;
				_front.height = 0;
				++_frameSequence;
			}

			void SetStatus(State a_state, std::string a_message)
			{
				std::scoped_lock lock(_statusMutex);
				_status.state = a_state;
				_status.message = std::move(a_message);
			}

			// Worker thread only.
			const Mpv::Api*     _api{ nullptr };
			mpv_handle*         _mpv{ nullptr };
			mpv_render_context* _render{ nullptr };
			std::string         _loadError;
			std::string         _ytdlError;  // last ytdl hook error for the current file
			std::string         _currentPath;  // URL or path of the current file, as reported by mpv
			std::string         _requestedSource;  // the last link or path passed to loadfile
			YouTubeSolver       _solver{ YouTubeSolver::Always };
			bool                _retryingWithQuickJs{ false };
			double              _position{ 0.0 };
			double              _duration{ 0.0 };
			bool                _awaitingFrame{ false };
			std::array<std::uint8_t, 256> _brightnessLut{};
			int                           _lutBrightness{ 0 };  // brightness _brightnessLut was built for; 0 = not applied
			int                 _volume{ 50 };            // the user's volume; mpv's is ramped around file changes
			bool                _fadeInPending{ false };  // a new file is silent until its playback starts
			bool                _audioPlaying{ false };   // a file is playing (from its first playback start to its end)  // between opening a file and rendering its first frame
			std::string         _title;  // media-title: the YouTube title, or the file name
			std::vector<Edition>                  _editions;
			std::int64_t                          _currentEdition{ -1 };
			std::chrono::steady_clock::time_point _loadedAt;
			std::optional<std::chrono::steady_clock::time_point> _recoveredAt;  // last RecoverStream reload
			int                 _videoWidth{ 0 };
			int                 _videoHeight{ 0 };
			bool                _paused{ false };
			FrameBuffer         _back;

			std::once_flag                    _started;
			std::mutex                        _queueMutex;
			std::condition_variable           _wakeCondition;
			std::deque<std::function<void()>> _commands;
			bool                              _wake{ false };

			std::mutex _statusMutex;
			Status     _status;

			std::mutex    _frameMutex;
			FrameBuffer   _front;
			std::uint64_t _frameSequence{ 0 };
		};
	}

	void Play(std::string a_source)
	{
		Player::Get().Play(std::move(a_source));
	}

	void TogglePause()
	{
		Player::Get().TogglePause();
	}

	void SetPaused(bool a_paused)
	{
		Player::Get().SetPaused(a_paused);
	}

	void Stop()
	{
		Player::Get().Stop();
	}

	void Reload()
	{
		Player::Get().Reload();
	}

	void SeekBy(double a_seconds)
	{
		Player::Get().SeekBy(a_seconds);
	}

	void SeekTo(double a_fraction)
	{
		Player::Get().SeekTo(a_fraction);
	}

	void SetVolume(int a_volume)
	{
		Player::Get().SetVolume(a_volume);
	}

	void SelectQuality(int a_height)
	{
		Player::Get().SelectQuality(a_height);
	}

	void SetYouTubeSolver(YouTubeSolver a_solver)
	{
		Player::Get().SetYouTubeSolver(a_solver);
	}

	void ShowNotice(std::string a_message)
	{
		Player::Get().ShowNotice(std::move(a_message));
	}

	Status GetStatus()
	{
		return Player::Get().GetStatus();
	}

	void ConsumeFrame(std::uint64_t& a_sequence, const std::function<void(const FrameView&)>& a_upload)
	{
		Player::Get().ConsumeFrame(a_sequence, a_upload);
	}
}
