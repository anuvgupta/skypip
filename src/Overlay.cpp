#include "Overlay.h"

#include "Keys.h"
#include "Panel.h"
#include "Settings.h"
#include "VideoPlayer.h"

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_internal.h>  // ImRect

namespace PiP::Overlay
{
	namespace
	{
		using Microsoft::WRL::ComPtr;

		constexpr float kDefaultAspect = 16.0F / 9.0F;

		std::atomic_bool g_initialized{ false };
		std::atomic_bool g_visible{ false };

		ID3D11Device*        g_device{ nullptr };
		ID3D11DeviceContext* g_context{ nullptr };

		// Latest video frame, owned by the render thread.
		ComPtr<ID3D11Texture2D>          g_texture;
		ComPtr<ID3D11ShaderResourceView> g_textureView;
		int                              g_textureWidth{ 0 };
		int                              g_textureHeight{ 0 };
		bool                             g_hasFrame{ false };
		std::uint64_t                    g_frameSequence{ 0 };

		std::chrono::steady_clock::time_point g_lastFrameTime;
		float                                 g_fade{ 0.0F };  // 0 = hidden, 1 = shown; eases toward the show/hide state
		constexpr float                       kFadeInSeconds = 0.15F;
		constexpr float                       kFadeOutSeconds = 0.08F;
		float                                 g_scale{ 1.0F };

		// Control panel input, fed by the input thread and applied by the render thread.
		struct PendingInput
		{
			enum class Kind
			{
				Key,
				MouseButton,
				Wheel,
				Char
			};

			Kind         kind;
			int          code{ 0 };  // ImGuiKey, mouse button index, or UTF-16 unit
			bool         pressed{ false };
			float        wheel{ 0.0F };
		};

		std::mutex                g_inputMutex;
		std::vector<PendingInput> g_pendingInput;
		ImVec2                    g_cursorDelta{ 0, 0 };
		ImVec2                    g_cursor{ 0, 0 };
		bool                      g_interacting{ false };  // render thread: panel was open last frame

		ImGuiKey ToImGuiKey(std::uint32_t a_scanCode)
		{
			switch (a_scanCode) {
			case 0x01: return ImGuiKey_Escape;
			case 0x0E: return ImGuiKey_Backspace;
			case 0x0F: return ImGuiKey_Tab;
			case 0x1C: return ImGuiKey_Enter;
			case 0x9C: return ImGuiKey_KeypadEnter;
			case 0x39: return ImGuiKey_Space;
			case 0x1D: return ImGuiKey_LeftCtrl;
			case 0x9D: return ImGuiKey_RightCtrl;
			case 0x2A: return ImGuiKey_LeftShift;
			case 0x36: return ImGuiKey_RightShift;
			case 0x38: return ImGuiKey_LeftAlt;
			case 0xB8: return ImGuiKey_RightAlt;
			// The Windows keys stand in for Ctrl so Cmd+V/C/X/A work when CrossOver forwards Cmd as the Windows key.
			case 0xDB: return ImGuiKey_LeftCtrl;
			case 0xDC: return ImGuiKey_RightCtrl;
			case 0xC7: return ImGuiKey_Home;
			case 0xCF: return ImGuiKey_End;
			case 0xC9: return ImGuiKey_PageUp;
			case 0xD1: return ImGuiKey_PageDown;
			case 0xD2: return ImGuiKey_Insert;
			case 0xD3: return ImGuiKey_Delete;
			case 0xC8: return ImGuiKey_UpArrow;
			case 0xD0: return ImGuiKey_DownArrow;
			case 0xCB: return ImGuiKey_LeftArrow;
			case 0xCD: return ImGuiKey_RightArrow;
			case 0x1E: return ImGuiKey_A;
			case 0x2E: return ImGuiKey_C;
			case 0x2F: return ImGuiKey_V;
			case 0x2D: return ImGuiKey_X;
			case 0x2C: return ImGuiKey_Z;
			case 0x15: return ImGuiKey_Y;
			default: return ImGuiKey_None;
			}
		}

		ImGuiKey ModifierFor(ImGuiKey a_key)
		{
			switch (a_key) {
			case ImGuiKey_LeftCtrl:
			case ImGuiKey_RightCtrl:
				return ImGuiMod_Ctrl;
			case ImGuiKey_LeftShift:
			case ImGuiKey_RightShift:
				return ImGuiMod_Shift;
			case ImGuiKey_LeftAlt:
			case ImGuiKey_RightAlt:
				return ImGuiMod_Alt;
			default:
				return ImGuiKey_None;
			}
		}

		// Starts or ends control panel interaction: software cursor, fresh input state.
		void SyncInteraction(bool a_panelOpen, ImVec2 a_screen)
		{
			if (a_panelOpen == g_interacting) {
				return;
			}
			g_interacting = a_panelOpen;

			auto& io = ImGui::GetIO();
			io.ClearInputKeys();
			io.ClearInputMouse();
			io.MouseDrawCursor = a_panelOpen;
			{
				std::scoped_lock lock(g_inputMutex);
				g_pendingInput.clear();
				g_cursorDelta = { 0, 0 };
			}
			if (a_panelOpen) {
				g_cursor = { a_screen.x * 0.5F, a_screen.y * 0.5F };
				io.AddMousePosEvent(g_cursor.x, g_cursor.y);
			} else {
				io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
			}
		}

		void ApplyPendingInput(ImVec2 a_screen)
		{
			std::vector<PendingInput> pending;
			ImVec2                    delta;
			{
				std::scoped_lock lock(g_inputMutex);
				pending.swap(g_pendingInput);
				delta = g_cursorDelta;
				g_cursorDelta = { 0, 0 };
			}

			auto& io = ImGui::GetIO();
			if (delta.x != 0.0F || delta.y != 0.0F) {
				g_cursor.x = std::clamp(g_cursor.x + delta.x, 0.0F, a_screen.x - 1.0F);
				g_cursor.y = std::clamp(g_cursor.y + delta.y, 0.0F, a_screen.y - 1.0F);
				io.AddMousePosEvent(g_cursor.x, g_cursor.y);
			}
			for (const auto& input : pending) {
				switch (input.kind) {
				case PendingInput::Kind::Key:
					if (const auto modifier = ModifierFor(static_cast<ImGuiKey>(input.code)); modifier != ImGuiKey_None) {
						io.AddKeyEvent(modifier, input.pressed);
					}
					io.AddKeyEvent(static_cast<ImGuiKey>(input.code), input.pressed);
					break;
				case PendingInput::Kind::MouseButton:
					io.AddMouseButtonEvent(input.code, input.pressed);
					break;
				case PendingInput::Kind::Wheel:
					io.AddMouseWheelEvent(0.0F, input.wheel);
					break;
				case PendingInput::Kind::Char:
					io.AddInputCharacterUTF16(static_cast<ImWchar16>(input.code));
					break;
				}
			}
		}

		void LoadFont(ImVec2 a_screen)
		{
			auto& io = ImGui::GetIO();
			g_scale = std::max(0.75F, a_screen.y / 1080.0F);

			char windows[MAX_PATH]{};
			::GetWindowsDirectoryA(windows, MAX_PATH);
			for (const auto* name : { "segoeui.ttf", "verdana.ttf", "arial.ttf" }) {
				const auto path = std::filesystem::path(windows) / "Fonts" / name;
				std::error_code error;
				if (std::filesystem::is_regular_file(path, error) &&
					io.Fonts->AddFontFromFileTTF(path.string().c_str(), 18.0F)) {
					logger::info("UI font: {}", path.string());
					break;
				}
			}

			auto& style = ImGui::GetStyle();
			ImGui::StyleColorsDark();
			style.WindowRounding = 8.0F;
			style.FrameRounding = 5.0F;
			style.GrabRounding = 5.0F;
			style.WindowPadding = { 12.0F, 12.0F };
			style.ItemSpacing = { 8.0F, 8.0F };
			style.Colors[ImGuiCol_WindowBg].w = 0.92F;
			style.ScaleAllSizes(g_scale);
			style.FontSizeBase = 18.0F;
			style.FontScaleMain = g_scale;
			style.MouseCursorScale = g_scale;
		}

		bool EnsureTexture(int a_width, int a_height)
		{
			if (g_texture && g_textureWidth == a_width && g_textureHeight == a_height) {
				return true;
			}

			g_textureView.Reset();
			g_texture.Reset();

			const D3D11_TEXTURE2D_DESC desc{
				.Width = static_cast<UINT>(a_width),
				.Height = static_cast<UINT>(a_height),
				.MipLevels = 1,
				.ArraySize = 1,
				.Format = DXGI_FORMAT_B8G8R8A8_UNORM,
				.SampleDesc = { 1, 0 },
				.Usage = D3D11_USAGE_DYNAMIC,
				.BindFlags = D3D11_BIND_SHADER_RESOURCE,
				.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
				.MiscFlags = 0,
			};
			if (FAILED(g_device->CreateTexture2D(&desc, nullptr, &g_texture)) ||
				FAILED(g_device->CreateShaderResourceView(g_texture.Get(), nullptr, &g_textureView))) {
				logger::error("Failed to create a {}x{} video texture", a_width, a_height);
				g_textureView.Reset();
				g_texture.Reset();
				return false;
			}

			g_textureWidth = a_width;
			g_textureHeight = a_height;
			return true;
		}

		void UploadLatestFrame()
		{
			VideoPlayer::ConsumeFrame(g_frameSequence, [](const VideoPlayer::FrameView& a_frame) {
				if (a_frame.width == 0 || !EnsureTexture(a_frame.width, a_frame.height)) {
					g_hasFrame = false;
					return;
				}

				D3D11_MAPPED_SUBRESOURCE mapped{};
				if (FAILED(g_context->Map(g_texture.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
					return;
				}
				const auto rowBytes = static_cast<std::size_t>(a_frame.width) * 4;
				auto*      dst = static_cast<std::uint8_t*>(mapped.pData);
				for (int y = 0; y < a_frame.height; ++y) {
					std::memcpy(dst + y * mapped.RowPitch, a_frame.pixels + y * a_frame.stride, rowBytes);
				}
				g_context->Unmap(g_texture.Get(), 0);
				g_hasFrame = true;
			});
		}

		ImVec2 PlayerOrigin(const Settings& a_settings, ImVec2 a_size, ImVec2 a_screen)
		{
			const bool left = a_settings.corner == Corner::TopLeft || a_settings.corner == Corner::BottomLeft;
			const bool top = a_settings.corner == Corner::TopLeft || a_settings.corner == Corner::TopRight;
			return {
				left ? static_cast<float>(a_settings.marginX) : a_screen.x - a_settings.marginX - a_size.x,
				top ? static_cast<float>(a_settings.marginY) : a_screen.y - a_settings.marginY - a_size.y
			};
		}

		// Draws the video (or the status box) and returns its screen rectangle.
		ImRect DrawPlayer(ImDrawList* a_drawList, ImVec2 a_screen, bool a_panelOpen)
		{
			const auto settings = Settings::Get();
			const auto status = VideoPlayer::GetStatus();

			const float  aspect = status.aspect > 0.0 ? static_cast<float>(status.aspect) : kDefaultAspect;
			const ImVec2 size{ static_cast<float>(settings.width), std::round(settings.width / aspect) };
			const ImVec2 drag = Panel::DragOffset();
			const ImVec2 origin = PlayerOrigin(settings, size, a_screen);
			const ImVec2 min{ origin.x + drag.x, origin.y + drag.y };
			const ImVec2 max{ min.x + size.x, min.y + size.y };
			const auto   alpha = static_cast<int>(settings.opacity * g_fade * 255.0F);

			const float fontSize = std::max(16.0F, a_screen.y / 1080.0F * 20.0F);
			const float padding = fontSize * 0.6F;

			if (g_hasFrame && (status.state == VideoPlayer::State::Playing || status.state == VideoPlayer::State::Paused)) {
				const auto texture = static_cast<ImTextureID>(reinterpret_cast<std::uintptr_t>(g_textureView.Get()));
				a_drawList->AddImageRounded(texture, min, max, { 0, 0 }, { 1, 1 }, IM_COL32(255, 255, 255, alpha), settings.cornerRadius);
				const auto drawShadowed = [&](float a_size, ImVec2 a_pos, const char* a_text) {
					a_drawList->AddText(nullptr, a_size, { a_pos.x + 1, a_pos.y + 1 }, IM_COL32(0, 0, 0, alpha), a_text);
					a_drawList->AddText(nullptr, a_size, a_pos, IM_COL32(255, 255, 255, alpha), a_text);
				};
				const ImVec2 textPos{ min.x + padding, max.y - padding - fontSize };
				if (status.buffering) {
					drawShadowed(fontSize, textPos, "Buffering...");
				} else if (status.state == VideoPlayer::State::Paused && !a_panelOpen) {
					// "Paused" on the left, a smaller hint on the right sharing its bottom edge.
					drawShadowed(fontSize, textPos, "Paused");
					const float hintSize = fontSize * 0.7F;
					const auto  hint = std::format("press {} to resume", Keys::KeyName(settings.togglePauseKey));
					const float hintWidth = ImGui::GetFont()->CalcTextSizeA(hintSize, FLT_MAX, 0.0F, hint.c_str()).x;
					drawShadowed(hintSize, { max.x - padding - hintWidth, textPos.y + fontSize - hintSize }, hint.c_str());
				}
				return { min, max };
			}

			std::string text = status.message;
			if (text.empty()) {
				text = a_panelOpen ? "No video loaded." :
				                     std::format("No video. Press {} to open the control panel.", Keys::KeyName(settings.togglePanelKey));
			}
			a_drawList->AddRectFilled(min, max, IM_COL32(0, 0, 0, alpha * 3 / 4), settings.cornerRadius);
			a_drawList->AddText(nullptr, fontSize, { min.x + padding, min.y + padding }, IM_COL32(255, 255, 255, alpha),
				text.c_str(), nullptr, size.x - 2 * padding);
			return { min, max };
		}
	}

	void Initialize()
	{
		const auto* renderer = RE::BSGraphics::Renderer::GetSingleton();
		if (!renderer) {
			logger::error("Renderer singleton is missing; overlay disabled");
			return;
		}
		const auto& data = renderer->GetRuntimeData();
		g_device = reinterpret_cast<ID3D11Device*>(data.forwarder);
		g_context = reinterpret_cast<ID3D11DeviceContext*>(data.context);
		if (!g_device || !g_context) {
			logger::error("D3D11 device or context is missing; overlay disabled");
			return;
		}

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		auto& io = ImGui::GetIO();
		io.IniFilename = nullptr;
		io.LogFilename = nullptr;

		if (!ImGui_ImplDX11_Init(g_device, g_context)) {
			logger::error("ImGui DX11 backend initialization failed; overlay disabled");
			return;
		}

		const auto screenSize = RE::BSGraphics::Renderer::GetScreenSize();
		LoadFont({ static_cast<float>(screenSize.width), static_cast<float>(screenSize.height) });

		g_lastFrameTime = std::chrono::steady_clock::now();
		g_initialized = true;
		logger::info("Overlay initialized (feature level {:#x})", static_cast<std::uint32_t>(g_device->GetFeatureLevel()));
	}

	void Render()
	{
		if (!g_initialized) {
			return;
		}

		const auto   screenSize = RE::BSGraphics::Renderer::GetScreenSize();
		const ImVec2 screen{ static_cast<float>(screenSize.width), static_cast<float>(screenSize.height) };

		const bool panelOpen = Panel::IsOpen();
		SyncInteraction(panelOpen, screen);

		// The clock advances even while hidden, so the next fade starts from a fresh frame time.
		const auto  now = std::chrono::steady_clock::now();
		const float deltaTime = std::max(std::chrono::duration<float>(now - g_lastFrameTime).count(), 1.0F / 1000.0F);
		g_lastFrameTime = now;

		const float elapsed = std::min(deltaTime, 0.1F);
		g_fade = (g_visible || panelOpen) ? std::min(1.0F, g_fade + elapsed / kFadeInSeconds) :
		                                    std::max(0.0F, g_fade - elapsed / kFadeOutSeconds);
		if (g_fade <= 0.0F) {
			return;
		}

		UploadLatestFrame();

		auto& io = ImGui::GetIO();
		io.DisplaySize = screen;
		io.DeltaTime = deltaTime;

		ApplyPendingInput(screen);

		ImGui_ImplDX11_NewFrame();
		ImGui::NewFrame();
		const auto player = DrawPlayer(ImGui::GetBackgroundDrawList(), screen, panelOpen);
		Panel::Draw(player.Min, player.Max, g_hasFrame);
		ImGui::Render();
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
	}

	void SetVisible(bool a_visible)
	{
		g_visible = a_visible;
	}

	void ToggleVisible()
	{
		g_visible = !g_visible;
	}

	void FeedInput(const RE::InputEvent& a_event)
	{
		std::scoped_lock lock(g_inputMutex);
		switch (a_event.GetEventType()) {
		case RE::INPUT_EVENT_TYPE::kMouseMove:
			if (const auto* move = a_event.AsMouseMoveEvent()) {
				g_cursorDelta.x += static_cast<float>(move->mouseInputX);
				g_cursorDelta.y += static_cast<float>(move->mouseInputY);
			}
			break;
		case RE::INPUT_EVENT_TYPE::kChar:
			if (const auto* character = a_event.AsCharEvent(); character && character->keyCode >= 0x20 && character->keyCode != 0x7F) {
				g_pendingInput.push_back({ .kind = PendingInput::Kind::Char, .code = static_cast<int>(character->keyCode) });
			}
			break;
		case RE::INPUT_EVENT_TYPE::kButton:
			{
				const auto* button = a_event.AsButtonEvent();
				if (!button) {
					break;
				}
				const auto code = button->GetIDCode();
				if (a_event.GetDevice() == RE::INPUT_DEVICE::kKeyboard) {
					if (const auto key = ToImGuiKey(code); key != ImGuiKey_None) {
						g_pendingInput.push_back({ .kind = PendingInput::Kind::Key, .code = key, .pressed = button->IsPressed() });
					}
				} else if (a_event.GetDevice() == RE::INPUT_DEVICE::kMouse) {
					using MouseKey = RE::BSWin32MouseDevice::Key;
					if (code == MouseKey::kWheelUp || code == MouseKey::kWheelDown) {
						if (button->IsDown()) {
							g_pendingInput.push_back({ .kind = PendingInput::Kind::Wheel, .wheel = code == MouseKey::kWheelUp ? 1.0F : -1.0F });
						}
					} else if (code < ImGuiMouseButton_COUNT) {
						g_pendingInput.push_back({ .kind = PendingInput::Kind::MouseButton, .code = static_cast<int>(code), .pressed = button->IsPressed() });
					}
				}
			}
			break;
		default:
			break;
		}
	}

	float Scale()
	{
		return g_scale;
	}
}
