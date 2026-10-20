#!/usr/bin/env bash
# Builds the release archives in build/package. Their root is the game's Data folder (SKSE/Plugins/...), the layout
# Nexus Mods, MO2, and Vortex expect; manual installs extract them into Data.
#   PiPPlayer-<version>.zip                 plugin + libmpv: local files and direct links
#   PiPPlayer-<version>-with-YouTube.zip    plugin + libmpv + yt-dlp + QuickJS: also YouTube links
#   PiPPlayer-YouTube-Tools-<version>.zip   yt-dlp + QuickJS only, to add YouTube support to the first archive
# The full set is also staged in build/package/stage for scripts/deploy-crossover.sh.
# Usage: scripts/package.sh [configure preset, default mac-release]
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
preset="${1:-mac-release}"
dll="$repo_root/build/$preset/PiPPlayer.dll"

if [[ ! -f "$dll" ]]; then
	echo "$dll not found. Build first: cmake --preset $preset && cmake --build --preset $preset" >&2
	exit 1
fi

"$repo_root/scripts/fetch-third-party.sh"

version="$(grep -m1 -E '^[[:space:]]*VERSION [0-9]+\.[0-9]+\.[0-9]+' "$repo_root/CMakeLists.txt" | awk '{print $2}')"
package_dir="$repo_root/build/package"
plugin_files=(
	"$dll:PiPPlayer.dll"
	"$repo_root/dist/PiPPlayer.ini:PiPPlayer.ini"
	"$repo_root/third_party/libmpv/libmpv-2.dll:PiPPlayer/libmpv-2.dll"
	"$repo_root/dist/THIRD_PARTY_NOTICES.txt:PiPPlayer/THIRD_PARTY_NOTICES.txt"
	"$repo_root/LICENSE:PiPPlayer/LICENSE.txt"
)
youtube_files=(
	"$repo_root/third_party/youtube/yt-dlp.exe:PiPPlayer/yt-dlp.exe"
	"$repo_root/third_party/youtube/qjs.exe:PiPPlayer/qjs.exe"
)

# stage <directory> <source:destination>...: copies files under <directory>/SKSE/Plugins.
stage() {
	local root="$1"
	shift
	rm -rf "$root"
	local entry
	for entry in "$@"; do
		local destination="$root/SKSE/Plugins/${entry##*:}"
		mkdir -p "$(dirname "$destination")"
		cp "${entry%%:*}" "$destination"
	done
}

# archive <directory> <zip name>
archive() {
	local zip="$package_dir/$2"
	rm -f "$zip"
	(cd "$1" && zip -qr "$zip" SKSE)
	echo "Package: $zip"
}

stage "$package_dir/plain" "${plugin_files[@]}"
archive "$package_dir/plain" "PiPPlayer-$version.zip"

stage "$package_dir/stage" "${plugin_files[@]}" "${youtube_files[@]}"
archive "$package_dir/stage" "PiPPlayer-$version-with-YouTube.zip"

stage "$package_dir/youtube-tools" "${youtube_files[@]}"
archive "$package_dir/youtube-tools" "PiPPlayer-YouTube-Tools-$version.zip"
