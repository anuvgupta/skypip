#include "Hooks.h"

#include "AutoPause.h"
#include "Input.h"
#include "Overlay.h"

namespace PiP::Hooks
{
	namespace
	{
		// Call inside BSGraphics::InitD3D, after the device and swap chain exist.
		struct InitD3D
		{
			static void thunk()
			{
				func();
				Overlay::Initialize();
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		// Timer call at the end of BSGraphics::Renderer::End, before the frame is presented.
		struct RendererEnd
		{
			static void thunk(std::uint32_t a_timer)
			{
				func(a_timer);
				AutoPause::Update();
				Overlay::Render();
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		// Call that hands each frame's input events to the game's input sinks (menus, controls, other plugins).
		struct DispatchInput
		{
			static void thunk(RE::BSTEventSource<RE::InputEvent*>* a_dispatcher, RE::InputEvent* const* a_events)
			{
				if (Input::Process(a_events)) {
					constexpr RE::InputEvent* const none[]{ nullptr };
					func(a_dispatcher, none);
				} else {
					func(a_dispatcher, a_events);
				}
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		template <class T>
		void WriteCall(REL::RelocationID a_id, REL::VariantOffset a_offset)
		{
			const REL::Relocation<std::uintptr_t> target{ a_id, a_offset };
			T::func = SKSE::GetTrampoline().write_call<5>(target.address(), T::thunk);
		}
	}

	void Install()
	{
		SKSE::AllocTrampoline(3 * 14);

		WriteCall<InitD3D>(RELOCATION_ID(75595, 77226), REL::VariantOffset(0x9, 0x275, 0x9));
		WriteCall<RendererEnd>(RELOCATION_ID(75461, 77246), REL::VariantOffset(0x9, 0x9, 0x15));
		WriteCall<DispatchInput>(RELOCATION_ID(67315, 68617), REL::VariantOffset(0x7B, 0x7B, 0x81));

		logger::info("Renderer and input hooks installed");
	}
}
