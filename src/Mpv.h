#pragma once

#include <mpv/client.h>
#include <mpv/render.h>

namespace PiP::Mpv
{
#define PIP_MPV_FUNCTIONS(X)                    \
	X(mpv_client_api_version)                   \
	X(mpv_error_string)                         \
	X(mpv_create)                               \
	X(mpv_initialize)                           \
	X(mpv_terminate_destroy)                    \
	X(mpv_set_option_string)                    \
	X(mpv_command_async)                        \
	X(mpv_observe_property)                     \
	X(mpv_request_log_messages)                 \
	X(mpv_wait_event)                           \
	X(mpv_set_wakeup_callback)                  \
	X(mpv_render_context_create)                \
	X(mpv_render_context_set_update_callback)   \
	X(mpv_render_context_update)                \
	X(mpv_render_context_render)                \
	X(mpv_render_context_free)

	// libmpv entry points, resolved at runtime so the plugin loads (and reports why) when libmpv is missing.
	struct Api
	{
#define PIP_MPV_MEMBER(a_name) decltype(&::a_name) a_name{ nullptr };
		PIP_MPV_FUNCTIONS(PIP_MPV_MEMBER)
#undef PIP_MPV_MEMBER
	};

	// Loads Data/SKSE/Plugins/PiPPlayer/libmpv-2.dll. Returns nullptr and fills a_error on failure.
	const Api* Load(std::string& a_error);
}
