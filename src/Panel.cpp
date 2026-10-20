#include "Panel.h"

#include "Clipboard.h"
#include "History.h"
#include "Keys.h"
#include "Overlay.h"
#include "Settings.h"
#include "VideoPlayer.h"
#include "YouTube.h"

namespace PiP::Panel
{
	namespace
	{
		using VideoPlayer::State;

		std::atomic_bool g_open{ false };
		bool             g_wasFrozen{ false };

		// Render thread state.
		std::array<char, 2048> g_url{};
		bool                   g_urlInitialized{ false };
		std::string            g_recentSelected;  // source of the chosen Recent entry; the most recent when not found
		ImVec2                 g_dragOffset{ 0, 0 };
		bool                   g_draggingVideo{ false };
		bool                   g_seeking{ false };
		float                  g_seekPreview{ 0.0F };
		// While a panel control is held, the panel stays put: it follows the video, and a size or margin slider
		// moving the video would otherwise move the slider out from under the cursor.
		bool   g_panelHeld{ false };
		ImVec2 g_panelPos{ 0, 0 };
		ImVec2 g_panelPivot{ 0, 0 };

		void SetFrozen(bool a_frozen)
		{
			auto* main = RE::Main::GetSingleton();
			if (!main) {
				return;
			}
			auto& freezeTime = main->GetRuntimeData().freezeTime;
			if (a_frozen) {
				g_wasFrozen = freezeTime;
				freezeTime = true;
			} else {
				freezeTime = g_wasFrozen;
			}
		}

		// The game keeps the system cursor hidden during play. Under CrossOver it can stay visible after the panel
		// closes until the window loses and regains focus, so hide it explicitly (no effect when already hidden).
		void HideSystemCursor()
		{
			while (::ShowCursor(FALSE) >= 0) {
			}
			::SetCursor(nullptr);
		}

		bool IsLoaded(const VideoPlayer::Status& a_status)
		{
			return a_status.state == State::Playing || a_status.state == State::Paused;
		}

		std::string FormatTime(double a_seconds)
		{
			const auto total = static_cast<std::int64_t>(std::max(a_seconds, 0.0));
			const auto hours = total / 3600;
			const auto minutes = total / 60 % 60;
			const auto seconds = total % 60;
			return hours > 0 ? std::format("{}:{:02}:{:02}", hours, minutes, seconds) : std::format("{}:{:02}", minutes, seconds);
		}

		void PlayUrl()
		{
			std::string_view url(g_url.data());
			while (!url.empty() && std::isspace(static_cast<unsigned char>(url.front()))) {
				url.remove_prefix(1);
			}
			while (!url.empty() && std::isspace(static_cast<unsigned char>(url.back()))) {
				url.remove_suffix(1);
			}
			if (url.empty()) {
				VideoPlayer::ShowNotice("Enter a video URL or file path.");
				return;
			}
			VideoPlayer::Play(std::string(url));
		}

		void SetUrl(std::string_view a_source)
		{
			const auto length = std::min(a_source.size(), g_url.size() - 1);
			std::memcpy(g_url.data(), a_source.data(), length);
			g_url[length] = '\0';
		}

		std::string RecentLabel(const History::Item& a_item)
		{
			std::string name = a_item.title.empty() ? a_item.source : a_item.title;
			if (name.size() > 60) {
				std::size_t cut = 57;
				while (cut > 0 && (static_cast<unsigned char>(name[cut]) & 0xC0) == 0x80) {  // not inside a UTF-8 character
					--cut;
				}
				name = name.substr(0, cut) + "...";
			}
			return a_item.position > 0.0 ? std::format("{}  ({})", name, FormatTime(a_item.position)) : name;
		}

		// Recently played videos, the most recent selected by default; Resume plays the selection from where it left off.
		void DrawRecent()
		{
			const auto items = History::Recent();
			if (items.empty()) {
				return;
			}
			auto selected = std::ranges::find(items, g_recentSelected, &History::Item::source);
			if (selected == items.end()) {
				selected = items.begin();
			}

			const auto& style = ImGui::GetStyle();
			const float resumeWidth = ImGui::CalcTextSize("Resume").x + style.FramePadding.x * 2.0F;
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted("Recent");
			ImGui::SameLine(96.0F * Overlay::Scale());
			ImGui::SetNextItemWidth(-(resumeWidth + style.ItemSpacing.x));
			if (ImGui::BeginCombo("##recent", RecentLabel(*selected).c_str())) {
				for (std::size_t i = 0; i < items.size(); ++i) {
					ImGui::PushID(static_cast<int>(i));
					if (ImGui::Selectable(RecentLabel(items[i]).c_str(), &items[i] == &*selected)) {
						g_recentSelected = items[i].source;
					}
					ImGui::PopID();
				}
				ImGui::EndCombo();
			}
			ImGui::SameLine();
			if (ImGui::Button("Resume")) {
				SetUrl(selected->source);
				VideoPlayer::Play(selected->source);
			}
		}

		void PlayClipboard()
		{
			if (auto source = Clipboard::ReadVideoSource()) {
				SetUrl(*source);
				VideoPlayer::Play(std::move(*source));
			} else {
				VideoPlayer::ShowNotice("The clipboard has no video URL or file path.");
			}
		}

		// Snaps the dragged video to the nearest corner, keeping its distance from the edges as the margins.
		void CommitDrag(ImVec2 a_min, ImVec2 a_max)
		{
			const ImVec2 screen = ImGui::GetIO().DisplaySize;
			const bool   left = (a_min.x + a_max.x) * 0.5F < screen.x * 0.5F;
			const bool   top = (a_min.y + a_max.y) * 0.5F < screen.y * 0.5F;
			Settings::Update([&](Settings& a_settings) {
				a_settings.corner = top ? (left ? Corner::TopLeft : Corner::TopRight) : (left ? Corner::BottomLeft : Corner::BottomRight);
				a_settings.marginX = std::max(0, static_cast<int>(std::lround(left ? a_min.x : screen.x - a_max.x)));
				a_settings.marginY = std::max(0, static_cast<int>(std::lround(top ? a_min.y : screen.y - a_max.y)));
			});
			g_dragOffset = { 0, 0 };
		}

		bool CircleButton(const char* a_id, ImVec2 a_center, float a_radius)
		{
			ImGui::SetCursorScreenPos({ a_center.x - a_radius, a_center.y - a_radius });
			const bool pressed = ImGui::InvisibleButton(a_id, { a_radius * 2, a_radius * 2 });
			const bool hovered = ImGui::IsItemHovered();
			ImGui::GetWindowDrawList()->AddCircleFilled(a_center, a_radius, IM_COL32(0, 0, 0, hovered ? 200 : 140));
			return pressed;
		}

		void DrawCenteredText(ImDrawList* a_drawList, ImVec2 a_center, float a_size, const char* a_text)
		{
			const ImVec2 extent = ImGui::GetFont()->CalcTextSizeA(a_size, FLT_MAX, 0.0F, a_text);
			a_drawList->AddText(nullptr, a_size, { a_center.x - extent.x * 0.5F, a_center.y - extent.y * 0.5F },
				IM_COL32(255, 255, 255, 235), a_text);
		}

		void DrawPlayPauseIcon(ImDrawList* a_drawList, ImVec2 a_center, float a_radius, bool a_showPlay)
		{
			const auto white = IM_COL32(255, 255, 255, 235);
			if (a_showPlay) {
				const float s = a_radius * 0.42F;
				a_drawList->AddTriangleFilled({ a_center.x - s * 0.6F, a_center.y - s }, { a_center.x - s * 0.6F, a_center.y + s },
					{ a_center.x + s, a_center.y }, white);
			} else {
				const float w = a_radius * 0.14F;
				const float h = a_radius * 0.4F;
				const float gap = a_radius * 0.12F;
				a_drawList->AddRectFilled({ a_center.x - gap - w, a_center.y - h }, { a_center.x - gap, a_center.y + h }, white);
				a_drawList->AddRectFilled({ a_center.x + gap, a_center.y - h }, { a_center.x + gap + w, a_center.y + h }, white);
			}
		}

		// The title along the top of the video, shortened with "..." to fit.
		void DrawTitle(ImDrawList* a_drawList, ImVec2 a_min, ImVec2 a_max, const std::string& a_title)
		{
			if (a_title.empty()) {
				return;
			}
			const float scale = Overlay::Scale();
			const float padding = 10.0F * scale;
			const float fontSize = ImGui::GetFontSize() * 0.95F;
			const float shade = std::min((a_max.y - a_min.y) * 0.3F, fontSize + 2.0F * padding);
			a_drawList->AddRectFilledMultiColor(a_min, { a_max.x, a_min.y + shade }, IM_COL32(0, 0, 0, 160), IM_COL32(0, 0, 0, 160),
				IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 0));

			auto*       font = ImGui::GetFont();
			const float available = a_max.x - a_min.x - 2.0F * padding;
			std::string text = a_title;
			if (font->CalcTextSizeA(fontSize, FLT_MAX, 0.0F, text.c_str()).x > available) {
				const float ellipsis = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0F, "...").x;
				while (!text.empty() && font->CalcTextSizeA(fontSize, FLT_MAX, 0.0F, text.c_str()).x + ellipsis > available) {
					text.pop_back();
					while (!text.empty() && (static_cast<unsigned char>(text.back()) & 0xC0) == 0x80) {  // whole UTF-8 characters
						text.pop_back();
					}
					if (!text.empty() && static_cast<unsigned char>(text.back()) >= 0xC0) {
						text.pop_back();
					}
				}
				text += "...";
			}
			const ImVec2 position{ a_min.x + padding, a_min.y + padding };
			a_drawList->AddText(nullptr, fontSize, { position.x + 1, position.y + 1 }, IM_COL32(0, 0, 0, 200), text.c_str());
			a_drawList->AddText(nullptr, fontSize, position, IM_COL32(255, 255, 255, 240), text.c_str());
		}

		void DrawSeekBar(ImDrawList* a_drawList, ImVec2 a_min, ImVec2 a_max, const VideoPlayer::Status& a_status)
		{
			const float scale = Overlay::Scale();
			const float padding = 10.0F * scale;
			const float barHeight = 4.0F * scale;
			const float hitHeight = 18.0F * scale;
			const float fontSize = ImGui::GetFontSize() * 0.85F;
			const float y = a_max.y - padding - hitHeight * 0.5F;
			const ImVec2 barMin{ a_min.x + padding, y - barHeight * 0.5F };
			const ImVec2 barMax{ a_max.x - padding, y + barHeight * 0.5F };
			const float  barWidth = barMax.x - barMin.x;
			const float  textY = barMin.y - fontSize - 4.0F * scale;

			if (a_status.duration <= 0.0) {
				a_drawList->AddText(nullptr, fontSize, { barMin.x, textY }, IM_COL32(255, 255, 255, 235), FormatTime(a_status.position).c_str());
				return;
			}

			ImGui::SetCursorScreenPos({ barMin.x, y - hitHeight * 0.5F });
			ImGui::InvisibleButton("##seek", { barWidth, hitHeight });
			const bool hovered = ImGui::IsItemHovered();
			if (ImGui::IsItemActive()) {
				g_seeking = true;
				g_seekPreview = std::clamp((ImGui::GetIO().MousePos.x - barMin.x) / barWidth, 0.0F, 1.0F);
			}
			if (ImGui::IsItemDeactivated() && g_seeking) {
				VideoPlayer::SeekTo(g_seekPreview);
				g_seeking = false;
			}

			const float fraction = g_seeking ? g_seekPreview :
			                                   std::clamp(static_cast<float>(a_status.position / a_status.duration), 0.0F, 1.0F);
			const float knobX = barMin.x + barWidth * fraction;
			a_drawList->AddRectFilled(barMin, barMax, IM_COL32(255, 255, 255, 70), barHeight);
			a_drawList->AddRectFilled(barMin, { knobX, barMax.y }, IM_COL32(230, 45, 45, 255), barHeight);
			if (hovered || g_seeking) {
				a_drawList->AddCircleFilled({ knobX, y }, barHeight * 1.8F, IM_COL32(230, 45, 45, 255));
			}

			const auto position = FormatTime(g_seeking ? g_seekPreview * a_status.duration : a_status.position);
			const auto duration = FormatTime(a_status.duration);
			const float durationWidth = ImGui::GetFont()->CalcTextSizeA(fontSize, FLT_MAX, 0.0F, duration.c_str()).x;
			a_drawList->AddText(nullptr, fontSize, { barMin.x, textY }, IM_COL32(255, 255, 255, 235), position.c_str());
			a_drawList->AddText(nullptr, fontSize, { barMax.x - durationWidth, textY }, IM_COL32(255, 255, 255, 235), duration.c_str());
		}

		// Controls on top of the video: drag to move, click to pause, play/pause and skip buttons, seek bar.
		void DrawVideoControls(ImVec2 a_min, ImVec2 a_max, bool a_hasFrame)
		{
			const auto   status = VideoPlayer::GetStatus();
			const ImVec2 size{ a_max.x - a_min.x, a_max.y - a_min.y };

			ImGui::SetNextWindowPos(a_min);
			ImGui::SetNextWindowSize(size);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0, 0 });
			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0F);
			constexpr auto flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove |
			                       ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
			                       ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoScrollWithMouse;
			ImGui::Begin("##pip-video", nullptr, flags);
			ImGui::PopStyleVar(2);
			auto* drawList = ImGui::GetWindowDrawList();

			ImGui::SetCursorScreenPos(a_min);
			ImGui::SetNextItemAllowOverlap();
			ImGui::InvisibleButton("##move", size);
			if (ImGui::IsItemActive()) {
				g_draggingVideo = g_draggingVideo || ImGui::IsMouseDragging(ImGuiMouseButton_Left);
				if (g_draggingVideo) {
					g_dragOffset = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
				}
			}
			if (ImGui::IsItemDeactivated()) {
				if (g_draggingVideo) {
					CommitDrag(a_min, a_max);
				} else if (IsLoaded(status)) {
					VideoPlayer::TogglePause();
				}
				g_draggingVideo = false;
			}
			if (ImGui::IsItemHovered() || g_draggingVideo) {
				ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
			}

			if (a_hasFrame && IsLoaded(status) && !g_draggingVideo) {
				const float scale = Overlay::Scale();
				const float shade = std::min(size.y * 0.45F, 72.0F * scale);
				drawList->AddRectFilledMultiColor({ a_min.x, a_max.y - shade }, a_max, IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 0),
					IM_COL32(0, 0, 0, 160), IM_COL32(0, 0, 0, 160));

				DrawTitle(drawList, a_min, a_max, status.title);

				const ImVec2 center{ (a_min.x + a_max.x) * 0.5F, a_min.y + (size.y - shade * 0.5F) * 0.5F };
				const float  radius = std::clamp(std::min(size.x, size.y) * 0.13F, 16.0F * scale, 44.0F * scale);
				const float  skipRadius = radius * 0.72F;
				const float  skipOffset = radius + skipRadius + 14.0F * scale;

				if (CircleButton("##play-pause", center, radius)) {
					VideoPlayer::TogglePause();
				}
				DrawPlayPauseIcon(drawList, center, radius, status.state == State::Paused);

				const ImVec2 back{ center.x - skipOffset, center.y };
				if (CircleButton("##back", back, skipRadius)) {
					VideoPlayer::SeekBy(-5.0);
				}
				DrawCenteredText(drawList, back, skipRadius * 0.85F, "-5");

				const ImVec2 forward{ center.x + skipOffset, center.y };
				if (CircleButton("##forward", forward, skipRadius)) {
					VideoPlayer::SeekBy(5.0);
				}
				DrawCenteredText(drawList, forward, skipRadius * 0.85F, "+5");

				DrawSeekBar(drawList, a_min, a_max, status);
			}
			ImGui::End();
		}

		// Label on the left, control filling the rest of the row.
		void Row(const char* a_label)
		{
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted(a_label);
			ImGui::SameLine(96.0F * Overlay::Scale());
			ImGui::SetNextItemWidth(-FLT_MIN);
		}

		std::string QualityLabel(int a_height)
		{
			return a_height > 0 ? std::format("{}p", a_height) : std::string("Best");
		}

		void DrawYouTubeSection(const Settings& a_settings, const VideoPlayer::Status& a_status)
		{
			// While a video offering several qualities plays, list its own; otherwise pick the default for the next one.
			const bool       live = !a_status.qualities.empty() && IsLoaded(a_status);
			std::vector<int> heights = live ? a_status.qualities : std::vector<int>{ 360, 480, 720, 1080 };
			heights.push_back(0);
			const int selected = live ? a_status.videoHeight : a_settings.youTubeMaxHeight;

			Row("Quality");
			if (ImGui::BeginCombo("##youtube-quality", QualityLabel(selected).c_str())) {
				for (const int height : heights) {
					if (ImGui::Selectable(QualityLabel(height).c_str(), height == selected)) {
						Settings::Update([&](Settings& a_edit) { a_edit.youTubeMaxHeight = height; });
						VideoPlayer::SelectQuality(height);
					}
				}
				ImGui::EndCombo();
			}
			ImGui::SetItemTooltip(live ? "Switch this video's quality (also the default for YouTube videos)" :
			                             "Highest quality requested for YouTube videos");

			constexpr std::tuple<const char*, YouTubeSolver, const char*> solvers[]{
				{ "Always", YouTubeSolver::Always, "Every lookup uses QuickJS (most reliable)" },
				{ "Fallback", YouTubeSolver::Fallback, "Skip QuickJS, retry with it if a lookup fails (faster)" },
				{ "Off", YouTubeSolver::Off, "Never use QuickJS (fastest; some videos may fail)" },
			};
			const auto current = std::ranges::find(solvers, a_settings.youTubeSolver, [](const auto& a_entry) { return std::get<1>(a_entry); });
			Row("JS solver");
			if (ImGui::BeginCombo("##youtube-solver", std::get<0>(*current))) {
				for (const auto& [label, solver, description] : solvers) {
					if (ImGui::Selectable(label, solver == a_settings.youTubeSolver)) {
						Settings::Update([&](Settings& a_edit) { a_edit.youTubeSolver = solver; });
						VideoPlayer::SetYouTubeSolver(solver);
					}
					ImGui::SetItemTooltip("%s", description);
				}
				ImGui::EndCombo();
			}
			ImGui::SetItemTooltip("How YouTube lookups use QuickJS, which solves YouTube's JavaScript challenge");

			ImGui::BeginDisabled(YouTube::IsUpdating());
			if (ImGui::Button("Update yt-dlp")) {
				YouTube::StartUpdate();
			}
			ImGui::EndDisabled();
			if (const auto status = YouTube::UpdateStatus(); !status.empty()) {
				ImGui::SameLine();
				ImGui::AlignTextToFramePadding();
				ImGui::TextWrapped("%s", status.c_str());
			}
		}

		void DrawPanelWindow(ImVec2 a_min, ImVec2 a_max)
		{
			const auto  settings = Settings::Get();
			const auto  status = VideoPlayer::GetStatus();
			const float scale = Overlay::Scale();
			const bool  videoLeft = settings.corner == Corner::TopLeft || settings.corner == Corner::BottomLeft;
			const bool  videoTop = settings.corner == Corner::TopLeft || settings.corner == Corner::TopRight;
			const float gap = 12.0F * scale;
			const float width = 380.0F * scale;

			if (!g_urlInitialized) {
				SetUrl(History::LastSource().value_or(settings.url));
				g_urlInitialized = true;
			}

			if (!g_panelHeld) {
				g_panelPos = { videoLeft ? a_max.x + gap : a_min.x - gap, videoTop ? a_min.y : a_max.y };
				g_panelPivot = { videoLeft ? 0.0F : 1.0F, videoTop ? 0.0F : 1.0F };
			}
			ImGui::SetNextWindowPos(g_panelPos, ImGuiCond_Always, g_panelPivot);
			ImGui::SetNextWindowSizeConstraints({ width, 0.0F }, { width, FLT_MAX });
			bool open = true;
			constexpr auto flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
			                       ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings;
			if (ImGui::Begin("PiP Player", &open, flags)) {
				const auto& style = ImGui::GetStyle();
				const float playWidth = ImGui::CalcTextSize("Play").x + style.FramePadding.x * 2.0F;
				ImGui::SetNextItemWidth(-(playWidth + style.ItemSpacing.x));
				if (ImGui::InputTextWithHint("##url", "Paste a YouTube or video URL, or a file path", g_url.data(), g_url.size(),
						ImGuiInputTextFlags_EnterReturnsTrue)) {
					PlayUrl();
				}
				ImGui::SameLine();
				if (ImGui::Button("Play")) {
					PlayUrl();
				}
				if (ImGui::Button("Play clipboard")) {
					PlayClipboard();
				}
				DrawRecent();
				if (!status.message.empty() && !IsLoaded(status)) {
					ImGui::PushStyleColor(ImGuiCol_Text, status.state == State::Error ? IM_COL32(255, 120, 110, 255) : IM_COL32(200, 200, 200, 255));
					ImGui::TextWrapped("%s", status.message.c_str());
					ImGui::PopStyleColor();
				}

				ImGui::Separator();
				ImGui::BeginDisabled(!IsLoaded(status));
				if (ImGui::Button(status.state == State::Paused ? "Resume" : "Pause")) {
					VideoPlayer::TogglePause();
				}
				ImGui::SameLine();
				if (ImGui::Button("Stop")) {
					VideoPlayer::Stop();
				}
				ImGui::SameLine();
				ImGui::AlignTextToFramePadding();
				const auto time = status.duration > 0.0 ? std::format("{} / {}", FormatTime(status.position), FormatTime(status.duration)) :
				                                          FormatTime(status.position);
				ImGui::TextUnformatted(time.c_str());
				ImGui::EndDisabled();

				int volume = settings.volume;
				Row("Volume");
				if (ImGui::SliderInt("##volume", &volume, 0, 100, "%d%%")) {
					Settings::Update([&](Settings& a_settings) { a_settings.volume = volume; });
					VideoPlayer::SetVolume(volume);
				}

				if (YouTube::Available()) {
					DrawYouTubeSection(settings, status);
				}

				ImGui::SeparatorText("Layout");
				constexpr std::pair<const char*, Corner> corners[]{
					{ "Top left", Corner::TopLeft },
					{ "Top right", Corner::TopRight },
					{ "Bottom left", Corner::BottomLeft },
					{ "Bottom right", Corner::BottomRight },
				};
				for (std::size_t i = 0; i < std::size(corners); ++i) {
					const auto [label, corner] = corners[i];
					if (i % 2 == 1) {
						ImGui::SameLine(width * 0.5F);
					}
					if (ImGui::RadioButton(label, settings.corner == corner)) {
						Settings::Update([&](Settings& a_settings) { a_settings.corner = corner; });
					}
				}

				int videoWidth = settings.width;
				Row("Size");
				if (ImGui::SliderInt("##width", &videoWidth, 160, 1280, "%d px")) {
					Settings::Update([&](Settings& a_settings) { a_settings.width = videoWidth; });
				}
				int opacity = static_cast<int>(std::lround(settings.opacity * 100.0F));
				Row("Opacity");
				if (ImGui::SliderInt("##opacity", &opacity, 5, 100, "%d%%")) {
					Settings::Update([&](Settings& a_settings) { a_settings.opacity = static_cast<float>(opacity) / 100.0F; });
				}
				int brightness = settings.brightness;
				Row("Brightness");
				if (ImGui::SliderInt("##brightness", &brightness, -100, 100, "%+d")) {
					Settings::Update([&](Settings& a_settings) { a_settings.brightness = brightness; });
				}
				int marginX = settings.marginX;
				Row("Margin X");
				if (ImGui::SliderInt("##margin-x", &marginX, 0, 600, "%d px")) {
					Settings::Update([&](Settings& a_settings) { a_settings.marginX = marginX; });
				}
				int marginY = settings.marginY;
				Row("Margin Y");
				if (ImGui::SliderInt("##margin-y", &marginY, 0, 600, "%d px")) {
					Settings::Update([&](Settings& a_settings) { a_settings.marginY = marginY; });
				}

				ImGui::Separator();
				const auto legend = std::format("{} or Esc: close   {}: pause   {}: hide\nDrag the video to move it.",
					Keys::KeyName(settings.togglePanelKey), Keys::KeyName(settings.togglePauseKey), Keys::KeyName(settings.toggleVisibleKey));
				ImGui::TextDisabled("%s", legend.c_str());
			}
			g_panelHeld = ImGui::IsAnyItemActive() && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
			ImGui::End();

			if (!open) {
				Close();
			}
		}
	}

	void Open()
	{
		if (g_open.exchange(true)) {
			return;
		}
		SetFrozen(true);
		Overlay::SetVisible(true);
		logger::info("Control panel opened");
	}

	void Close()
	{
		if (!g_open.exchange(false)) {
			return;
		}
		SetFrozen(false);
		HideSystemCursor();
		Settings::Save();
		History::Save(true);
		logger::info("Control panel closed");
	}

	bool IsOpen()
	{
		return g_open;
	}

	void Draw(ImVec2 a_videoMin, ImVec2 a_videoMax, bool a_hasFrame)
	{
		if (!g_open) {
			g_draggingVideo = false;
			g_seeking = false;
			g_panelHeld = false;
			g_dragOffset = { 0, 0 };
			return;
		}
		DrawVideoControls(a_videoMin, a_videoMax, a_hasFrame);
		DrawPanelWindow(a_videoMin, a_videoMax);
	}

	ImVec2 DragOffset()
	{
		return g_dragOffset;
	}
}
