cmake_minimum_required(VERSION 3.24)

project(
	PiPPlayer
	VERSION 1.0.4
	DESCRIPTION "Picture-in-picture video player for Skyrim SE / Enderal SE"
	LANGUAGES C CXX
)
set(PROJECT_FRIENDLY_NAME "PiP Player")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Static CRT: the plugin must not depend on the user's VC++ redistributable.
if(NOT DEFINED CMAKE_MSVC_RUNTIME_LIBRARY)
	set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

add_compile_definitions(NOMINMAX _USE_MATH_DEFINES WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
add_compile_options(/EHsc /permissive-)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	add_compile_options(-Wno-overloaded-virtual -Wno-delete-non-abstract-non-virtual-dtor)
else()
	add_compile_options(/Zc:preprocessor /utf-8)
endif()

include(cmake/Dependencies.cmake)

configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.rc.in"
	"${CMAKE_CURRENT_BINARY_DIR}/version.rc"
	@ONLY
)

get_target_property(COMMONLIB_SRC_DIR CommonLibSSE SOURCE_DIR)
include("${COMMONLIB_SRC_DIR}/cmake/CommonLibSSE.cmake")

add_commonlibsse_plugin(
	${PROJECT_NAME}
	SOURCES
	src/AutoPause.cpp
	src/Clipboard.cpp
	src/History.cpp
	src/Hooks.cpp
	src/Input.cpp
	src/Keys.cpp
	src/Main.cpp
	src/Mpv.cpp
	src/Overlay.cpp
	src/Panel.cpp
	src/Paths.cpp
	src/Settings.cpp
	src/VideoPlayer.cpp
	src/YouTube.cpp
	"${CMAKE_CURRENT_BINARY_DIR}/version.rc"
)

target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(${PROJECT_NAME} PRIVATE spdlog::spdlog xbyak simpleini imgui mpv_headers)
target_precompile_headers(${PROJECT_NAME} PRIVATE src/PCH.h)
