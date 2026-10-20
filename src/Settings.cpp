#include "Settings.h"

#include "Keys.h"

#include <SimpleIni.h>

namespace PiP
{
	namespace
	{
		constexpr auto kPath = "Data/SKSE/Plugins/PiPPlayer.ini";

		constexpr std::pair<std::string_view, YouTubeSolver> kSolvers[]{
			{ "Always"sv, YouTubeSolver::Always },
			{ "Fallback"sv, YouTubeSolver::Fallback },
			{ "Off"sv, YouTubeSolver::Off },
		};

		constexpr std::pair<std::string_view, Corner> kCorners[]{
			{ "TopLeft"sv, Corner::TopLeft },
			{ "TopRight"sv, Corner::TopRight },
			{ "BottomLeft"sv, Corner::BottomLeft },
			{ "BottomRight"sv, Corner::BottomRight },
		};

		std::mutex g_mutex;
		Settings   g_settings;
		bool       g_dirty{ false };

		Corner ParseCorner(std::string_view a_value, Corner a_default)
		{
			for (const auto& [name, corner] : kCorners) {
				if (_strnicmp(a_value.data(), name.data(), name.size()) == 0 && a_value.size() == name.size()) {
					return corner;
				}
			}
			logger::warn("Unknown Corner '{}', using the default", a_value);
			return a_default;
		}

		template <class T>
		T Clamped(const char* a_key, T a_value, T a_min, T a_max)
		{
			const T clamped = std::clamp(a_value, a_min, a_max);
			if (clamped != a_value) {
				logger::warn("{} = {} is out of range, using {}", a_key, a_value, clamped);
			}
			return clamped;
		}

		std::uint32_t ReadKey(const CSimpleIniA& a_ini, const char* a_name, std::uint32_t a_default)
		{
			const char* value = a_ini.GetValue("Hotkeys", a_name, nullptr);
			if (!value) {
				return a_default;
			}
			if (const auto key = Keys::ParseKey(value)) {
				return *key;
			}
			logger::warn("{} = '{}' is not a key name or code, using {}", a_name, value, Keys::KeyName(a_default));
			return a_default;
		}
	}

	void Settings::Load()
	{
		CSimpleIniA ini;
		ini.SetUnicode();
		if (ini.LoadFile(kPath) < 0) {
			logger::warn("{} not found, using defaults", kPath);
			return;
		}

		Settings s;

		s.corner = ParseCorner(ini.GetValue("Overlay", "Corner", "BottomLeft"), s.corner);
		s.width = Clamped("Width", static_cast<int>(ini.GetLongValue("Overlay", "Width", s.width)), 64, 3840);
		s.marginX = Clamped("MarginX", static_cast<int>(ini.GetLongValue("Overlay", "MarginX", s.marginX)), 0, 2000);
		s.marginY = Clamped("MarginY", static_cast<int>(ini.GetLongValue("Overlay", "MarginY", s.marginY)), 0, 2000);
		s.opacity = Clamped("Opacity", static_cast<float>(ini.GetDoubleValue("Overlay", "Opacity", s.opacity)), 0.05F, 1.0F);
		s.brightness = Clamped("Brightness", static_cast<int>(ini.GetLongValue("Overlay", "Brightness", s.brightness)), -100, 100);
		s.cornerRadius = Clamped("CornerRadius", static_cast<float>(ini.GetDoubleValue("Overlay", "CornerRadius", s.cornerRadius)), 0.0F, 64.0F);

		s.volume = Clamped("Volume", static_cast<int>(ini.GetLongValue("Playback", "Volume", s.volume)), 0, 100);
		s.loop = ini.GetBoolValue("Playback", "Loop", s.loop);
		s.url = ini.GetValue("Playback", "Url", "");
		const std::string_view solver = ini.GetValue("Playback", "YouTubeSolver", "Always");
		if (const auto match = std::ranges::find_if(kSolvers, [&](const auto& a_entry) {
				return a_entry.first.size() == solver.size() && _strnicmp(a_entry.first.data(), solver.data(), solver.size()) == 0;
			});
			match != std::end(kSolvers)) {
			s.youTubeSolver = match->second;
		} else {
			logger::warn("Unknown YouTubeSolver '{}', using Always", solver);
		}
		s.resumeOnStart = ini.GetBoolValue("Playback", "ResumeOnStart", s.resumeOnStart);
		s.youTubeMaxHeight = Clamped("YouTubeMaxHeight", static_cast<int>(ini.GetLongValue("Playback", "YouTubeMaxHeight", s.youTubeMaxHeight)), 0, 4320);

		s.togglePanelKey = ReadKey(ini, "TogglePanel", s.togglePanelKey);
		s.togglePauseKey = ReadKey(ini, "TogglePause", s.togglePauseKey);
		s.toggleVisibleKey = ReadKey(ini, "ToggleVisible", s.toggleVisibleKey);
		if (s.togglePanelKey == s.togglePauseKey || s.togglePanelKey == s.toggleVisibleKey || s.togglePauseKey == s.toggleVisibleKey) {
			logger::warn("Two hotkeys share a key; only the first matching action (TogglePanel, TogglePause, ToggleVisible) runs");
		}

		logger::info("Settings loaded: width {}, margins {}x{}, opacity {:.2f}, volume {}, loop {}, keys panel {} / pause {} / show {}",
			s.width, s.marginX, s.marginY, s.opacity, s.volume, s.loop,
			Keys::KeyName(s.togglePanelKey), Keys::KeyName(s.togglePauseKey), Keys::KeyName(s.toggleVisibleKey));

		std::scoped_lock lock(g_mutex);
		g_settings = std::move(s);
		g_dirty = false;
	}

	void Settings::Save()
	{
		Settings s;
		{
			std::scoped_lock lock(g_mutex);
			if (!g_dirty) {
				return;
			}
			s = g_settings;
			g_dirty = false;
		}

		CSimpleIniA ini;
		ini.SetUnicode();
		ini.SetSpaces(false);
		ini.LoadFile(kPath);  // keeps the file's comments; a missing file is created

		const auto corner = std::ranges::find(kCorners, s.corner, &std::pair<std::string_view, Corner>::second)->first;
		ini.SetValue("Overlay", "Corner", std::string(corner).c_str());
		ini.SetValue("Overlay", "Width", std::to_string(s.width).c_str());
		ini.SetValue("Overlay", "MarginX", std::to_string(s.marginX).c_str());
		ini.SetValue("Overlay", "MarginY", std::to_string(s.marginY).c_str());
		ini.SetValue("Overlay", "Opacity", std::format("{:.2f}", s.opacity).c_str());
		ini.SetValue("Overlay", "Brightness", std::to_string(s.brightness).c_str());
		ini.SetValue("Playback", "Volume", std::to_string(s.volume).c_str());
		ini.SetValue("Playback", "YouTubeMaxHeight", std::to_string(s.youTubeMaxHeight).c_str());
		const auto solver = std::ranges::find(kSolvers, s.youTubeSolver, &std::pair<std::string_view, YouTubeSolver>::second)->first;
		ini.SetValue("Playback", "YouTubeSolver", std::string(solver).c_str());

		if (ini.SaveFile(kPath) < 0) {
			logger::error("Could not save {}", kPath);
			return;
		}
		logger::info("Saved settings: {} width {}, margins {}x{}, opacity {:.2f}, volume {}", corner, s.width, s.marginX,
			s.marginY, s.opacity, s.volume);
	}

	Settings Settings::Get()
	{
		std::scoped_lock lock(g_mutex);
		return g_settings;
	}

	void Settings::Update(const std::function<void(Settings&)>& a_edit)
	{
		std::scoped_lock lock(g_mutex);
		a_edit(g_settings);
		g_dirty = true;
	}
}
