#!/usr/bin/env bash
# Downloads the pinned runtime files the mod bundles into third_party/ and verifies their SHA-256:
#   third_party/libmpv/libmpv-2.dll   LGPL libmpv (github.com/zhongfly/mpv-winbuild)
#   third_party/youtube/yt-dlp.exe    yt-dlp (github.com/yt-dlp/yt-dlp)
#   third_party/youtube/qjs.exe       QuickJS-NG, the JavaScript runtime yt-dlp needs for YouTube
#
# zhongfly/mpv-winbuild keeps roughly a month of releases. When the libmpv pin disappears, pick a current
# "mpv-dev-lgpl-x86_64-*.7z" there and pass it with LIBMPV_URL and LIBMPV_SHA256 (or update the defaults below).
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
third_party="$repo_root/third_party"

libmpv_url="${LIBMPV_URL:-https://github.com/zhongfly/mpv-winbuild/releases/download/2026-10-18-fffe7a23a8/mpv-dev-lgpl-x86_64-20261018-git-fffe7a23a8.7z}"
libmpv_sha256="${LIBMPV_SHA256:-cd2f8190f63cc8b82c3abddfe317d1de9866c1ad6c5181092a2e75d7376ac4d7}"
ytdlp_url="https://github.com/yt-dlp/yt-dlp/releases/download/2026.08.19/yt-dlp.exe"
ytdlp_sha256="66674953fe251b89f4d08c5f0e35e0728679bd67ab3d7d05c0562af101dd3e7a"
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

# libmpv ships as a 7z archive; bsdtar (macOS, Windows 10+ tar.exe) reads it.
libmpv_dir="$third_party/libmpv"
if [[ ! -f "$libmpv_dir/libmpv-2.dll" || "$(cat "$libmpv_dir/.sha256" 2>/dev/null)" != "$libmpv_sha256" ]]; then
	archive="$third_party/$(basename "$libmpv_url")"
	download "$libmpv_url" "$libmpv_sha256" "$archive"
	rm -rf "$libmpv_dir"
	mkdir -p "$libmpv_dir"
	tar -xf "$archive" -C "$libmpv_dir"
	if [[ ! -f "$libmpv_dir/libmpv-2.dll" ]]; then
		echo "libmpv-2.dll not found in $archive" >&2
		exit 1
	fi
	echo "$libmpv_sha256" >"$libmpv_dir/.sha256"
fi

download "$ytdlp_url" "$ytdlp_sha256" "$third_party/youtube/yt-dlp.exe"
download "$qjs_url" "$qjs_sha256" "$third_party/youtube/qjs.exe"

echo "Third-party files ready in $third_party"
