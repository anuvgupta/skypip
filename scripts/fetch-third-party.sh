#!/usr/bin/env bash
# Downloads the pinned runtime files the mod bundles into third_party/ and verifies their SHA-256:
#   third_party/libmpv/libmpv-2.dll   LGPL libmpv (github.com/zhongfly/mpv-winbuild)
#   third_party/youtube/yt-dlp/       yt-dlp (github.com/yt-dlp/yt-dlp), the folder build: yt-dlp.exe + _internal,
#                                     which starts faster than the single-file yt-dlp.exe (no unpacking per run)
#   third_party/youtube/qjs.exe       QuickJS-NG, the JavaScript runtime yt-dlp needs for YouTube
#
# zhongfly/mpv-winbuild keeps roughly a month of releases. When the libmpv pin disappears, pick a current
# "mpv-dev-lgpl-x86_64-*.7z" there and pass it with LIBMPV_URL and LIBMPV_SHA256 (or update the defaults below).
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
third_party="$repo_root/third_party"

libmpv_url="${LIBMPV_URL:-https://github.com/zhongfly/mpv-winbuild/releases/download/2026-10-18-fffe7a23a8/mpv-dev-lgpl-x86_64-20261018-git-fffe7a23a8.7z}"
libmpv_sha256="${LIBMPV_SHA256:-cd2f8190f63cc8b82c3abddfe317d1de9866c1ad6c5181092a2e75d7376ac4d7}"
ytdlp_url="https://github.com/yt-dlp/yt-dlp/releases/download/2026.08.19/yt-dlp_win.zip"
ytdlp_sha256="30b4c14aafab6082becff7881e41b76df46dc43ea7633479410a91e29da492bf"
qjs_url="https://github.com/quickjs-ng/quickjs/releases/download/v0.17.0/qjs-windows-x86_64.exe"
qjs_sha256="2aeabf0092c3262d6b2609824418f7dd7ed1f1df939f73b2b15645230cac0d77"

sha256_of() {
	if command -v sha256sum >/dev/null; then
		sha256sum "$1" | cut -d' ' -f1
	else
		shasum -a 256 "$1" | cut -d' ' -f1
	fi
}

# download <url> <sha256> <destination file>: skips the download when the file already matches.
download() {
	local url="$1" sha256="$2" file="$3"
	if [[ -f "$file" && "$(sha256_of "$file")" == "$sha256" ]]; then
		return
	fi
	echo "Downloading $url"
	mkdir -p "$(dirname "$file")"
	curl -fL --retry 3 -o "$file.part" "$url"
	local actual
	actual="$(sha256_of "$file.part")"
	if [[ "$actual" != "$sha256" ]]; then
		rm -f "$file.part"
		echo "SHA-256 mismatch for $url: expected $sha256, got $actual" >&2
		exit 1
	fi
	mv "$file.part" "$file"
}

# unpack <url> <sha256> <directory> <file expected inside>: downloads and extracts an archive once per pin.
# bsdtar (macOS, Windows 10+ tar.exe) reads both 7z and zip.
unpack() {
	local url="$1" sha256="$2" directory="$3" expected="$4"
	if [[ -f "$directory/$expected" && "$(cat "$directory/.sha256" 2>/dev/null)" == "$sha256" ]]; then
		return
	fi
	local archive="$third_party/$(basename "$url")"
	download "$url" "$sha256" "$archive"
	rm -rf "$directory"
	mkdir -p "$directory"
	tar -xf "$archive" -C "$directory"
	if [[ ! -f "$directory/$expected" ]]; then
		echo "$expected not found in $archive" >&2
		exit 1
	fi
	echo "$sha256" >"$directory/.sha256"
}

unpack "$libmpv_url" "$libmpv_sha256" "$third_party/libmpv" libmpv-2.dll
unpack "$ytdlp_url" "$ytdlp_sha256" "$third_party/youtube/yt-dlp" yt-dlp.exe
download "$qjs_url" "$qjs_sha256" "$third_party/youtube/qjs.exe"

echo "Third-party files ready in $third_party"
