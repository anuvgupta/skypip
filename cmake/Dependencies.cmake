# Pinned third-party sources, fetched at configure time (no vcpkg).

include(FetchContent)

# SOURCE_SUBDIR no-cmake-project: populate the sources without adding the upstream CMake project.

# DirectXMath: required by DirectXTK's SimpleMath.
FetchContent_Declare(
	DirectXMath
	URL "https://github.com/microsoft/DirectXMath/archive/refs/tags/apr2025.tar.gz"
	DOWNLOAD_EXTRACT_TIMESTAMP ON
	OVERRIDE_FIND_PACKAGE
	EXCLUDE_FROM_ALL
	SYSTEM
)
FetchContent_MakeAvailable(DirectXMath)
add_library(Microsoft::DirectXMath ALIAS DirectXMath)

# DirectXTK: CommonLibSSE only uses SimpleMath (RE/S/State.h). The full library compiles its HLSL with fxc.exe,
# which does not exist off Windows, so build SimpleMath alone and expose it as the package CommonLibSSE asks for.
FetchContent_Declare(
	DirectXTK
	URL "https://github.com/microsoft/DirectXTK/archive/refs/tags/jul2025.tar.gz"
	DOWNLOAD_EXTRACT_TIMESTAMP ON
	SOURCE_SUBDIR no-cmake-project
)
FetchContent_MakeAvailable(DirectXTK)
add_library(DirectXTK STATIC "${directxtk_SOURCE_DIR}/Src/SimpleMath.cpp")
target_include_directories(DirectXTK
	SYSTEM PUBLIC "$<BUILD_INTERFACE:${directxtk_SOURCE_DIR}/Inc>"
	PRIVATE "${directxtk_SOURCE_DIR}/Src"
)
target_link_libraries(DirectXTK PUBLIC DirectXMath)
add_library(Microsoft::DirectXTK ALIAS DirectXTK)
install(TARGETS DirectXTK EXPORT DirectXTK-targets)
install(EXPORT DirectXTK-targets DESTINATION share/directxtk)
file(WRITE "${CMAKE_FIND_PACKAGE_REDIRECTS_DIR}/directxtk-config.cmake" "")

# rapidcsv: header-only, found by CommonLibSSE through RAPIDCSV_INCLUDE_DIRS.
FetchContent_Declare(
	rapidcsv
	URL "https://github.com/d99kris/rapidcsv/archive/refs/tags/v8.90.tar.gz"
	DOWNLOAD_EXTRACT_TIMESTAMP ON
	OVERRIDE_FIND_PACKAGE
)
FetchContent_MakeAvailable(rapidcsv)
set(RAPIDCSV_INCLUDE_DIRS "${rapidcsv_SOURCE_DIR}/src")

# spdlog: logging for the plugin and CommonLibSSE.
set(SPDLOG_INSTALL ON CACHE BOOL "" FORCE)
set(SPDLOG_USE_STD_FORMAT ON CACHE BOOL "" FORCE)
FetchContent_Declare(
	spdlog
	URL "https://github.com/gabime/spdlog/archive/refs/tags/v1.17.0.tar.gz"
	DOWNLOAD_EXTRACT_TIMESTAMP ON
	OVERRIDE_FIND_PACKAGE
)
FetchContent_MakeAvailable(spdlog)

# xbyak: CommonLibSSE trampoline support.
FetchContent_Declare(
	xbyak
	URL "https://github.com/herumi/xbyak/archive/v7.30.tar.gz"
	DOWNLOAD_EXTRACT_TIMESTAMP ON
)
FetchContent_MakeAvailable(xbyak)

# CommonLibSSE-GG: MIT fork of CommonLibSSE-NG with Skyrim 1.7.99 / 1.7.104 support.
set(SKSE_SUPPORT_XBYAK ON CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_SE ON CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_AE ON CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_VR OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
	CommonLibSSE
	GIT_REPOSITORY "https://github.com/eddoursul/CommonLibSSE-GG"
	GIT_TAG 2053e94fd1c147b36eae2b4338118552fba407e2
)
FetchContent_MakeAvailable(CommonLibSSE)
target_link_libraries(CommonLibSSE PUBLIC xbyak)

# simpleini: INI settings.
FetchContent_Declare(
	simpleini
	URL "https://github.com/brofield/simpleini/archive/refs/tags/v4.25.tar.gz"
	DOWNLOAD_EXTRACT_TIMESTAMP ON
	SOURCE_SUBDIR no-cmake-project
)
FetchContent_MakeAvailable(simpleini)
add_library(simpleini INTERFACE)
target_include_directories(simpleini SYSTEM INTERFACE "${simpleini_SOURCE_DIR}")

# Dear ImGui: overlay drawing through its DX11 backend.
FetchContent_Declare(
	imgui
	URL "https://github.com/ocornut/imgui/archive/refs/tags/v1.92.9.tar.gz"
	DOWNLOAD_EXTRACT_TIMESTAMP ON
)
FetchContent_MakeAvailable(imgui)
add_library(imgui STATIC
	"${imgui_SOURCE_DIR}/imgui.cpp"
	"${imgui_SOURCE_DIR}/imgui_draw.cpp"
	"${imgui_SOURCE_DIR}/imgui_tables.cpp"
	"${imgui_SOURCE_DIR}/imgui_widgets.cpp"
	"${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp"
)
target_include_directories(imgui SYSTEM PUBLIC "${imgui_SOURCE_DIR}" "${imgui_SOURCE_DIR}/backends")
target_link_libraries(imgui PUBLIC d3d11.lib d3dcompiler.lib dxgi.lib)

# mpv client headers only; libmpv-2.dll is loaded at runtime (scripts/fetch-third-party.sh provides the binary).
FetchContent_Declare(
	mpv
	URL "https://github.com/mpv-player/mpv/archive/refs/tags/v0.41.0.tar.gz"
	DOWNLOAD_EXTRACT_TIMESTAMP ON
	SOURCE_SUBDIR no-cmake-project
)
FetchContent_MakeAvailable(mpv)
add_library(mpv_headers INTERFACE)
target_include_directories(mpv_headers SYSTEM INTERFACE "${mpv_SOURCE_DIR}/include")
