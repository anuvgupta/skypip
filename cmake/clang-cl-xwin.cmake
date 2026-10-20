# Cross-compiles the Windows x64 plugin on macOS with LLVM clang-cl + lld-link.
#
# The MSVC CRT and Windows SDK come from an xwin splat made with
#   xwin --accept-license splat --use-winsysroot-style --preserve-ms-arch-notation --output <dir>
# Point XWIN_ROOT at <dir> (environment or cache variable). LLVM_ROOT defaults to Homebrew's keg-only llvm;
# lld-link comes from Homebrew's separate lld formula.

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR AMD64)

if(NOT XWIN_ROOT)
	set(XWIN_ROOT "$ENV{XWIN_ROOT}")
endif()
if(NOT XWIN_ROOT OR NOT EXISTS "${XWIN_ROOT}/VC/Tools/MSVC")
	message(FATAL_ERROR "Set XWIN_ROOT to an xwin splat made with --use-winsysroot-style (missing ${XWIN_ROOT}/VC/Tools/MSVC).")
endif()
set(XWIN_ROOT "${XWIN_ROOT}" CACHE PATH "xwin winsysroot-style MSVC CRT + Windows SDK")

if(NOT LLVM_ROOT)
	set(LLVM_ROOT "$ENV{LLVM_ROOT}")
endif()
if(NOT LLVM_ROOT)
	set(LLVM_ROOT "/opt/homebrew/opt/llvm")
endif()
set(LLVM_ROOT "${LLVM_ROOT}" CACHE PATH "LLVM installation providing clang-cl, llvm-lib, llvm-rc, llvm-mt")

find_program(LLD_LINK lld-link HINTS "${LLVM_ROOT}/bin" /opt/homebrew/bin REQUIRED)

set(CMAKE_C_COMPILER "${LLVM_ROOT}/bin/clang-cl")
set(CMAKE_CXX_COMPILER "${LLVM_ROOT}/bin/clang-cl")
set(CMAKE_LINKER "${LLD_LINK}")
set(CMAKE_AR "${LLVM_ROOT}/bin/llvm-lib")
set(CMAKE_RC_COMPILER "${LLVM_ROOT}/bin/llvm-rc")
set(CMAKE_MT "${LLVM_ROOT}/bin/llvm-mt")

set(_pip_target_flags "--target=x86_64-pc-windows-msvc /winsysroot \"${XWIN_ROOT}\"")
set(CMAKE_C_FLAGS_INIT "${_pip_target_flags}")
set(CMAKE_CXX_FLAGS_INIT "${_pip_target_flags}")

set(_pip_link_flags "/winsysroot:\"${XWIN_ROOT}\"")
set(CMAKE_EXE_LINKER_FLAGS_INIT "${_pip_link_flags}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "${_pip_link_flags}")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "${_pip_link_flags}")

# llvm-rc does not know the SDK layout; give it the headers winres.h needs.
file(GLOB _pip_sdk_include_dirs LIST_DIRECTORIES true "${XWIN_ROOT}/Windows Kits/10/Include/*")
list(SORT _pip_sdk_include_dirs)
list(GET _pip_sdk_include_dirs -1 _pip_sdk_include)
set(CMAKE_RC_FLAGS_INIT "-I \"${_pip_sdk_include}/um\" -I \"${_pip_sdk_include}/shared\"")

# xwin ships no debug CRT: keep try_compile and every configuration on the release static CRT.
set(CMAKE_TRY_COMPILE_CONFIGURATION Release)
set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreaded)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
