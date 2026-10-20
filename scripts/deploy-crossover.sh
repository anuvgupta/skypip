#!/usr/bin/env bash
# Copies the staged full package (scripts/package.sh, with the YouTube tools) into a CrossOver bottle's game folder.
# Writes only PiPPlayer files. An existing PiPPlayer.ini is kept unless --replace-ini is given.
#
# Environment: CROSSOVER_BOTTLE (default Steam), GAME_DIR (default: Enderal Special Edition in that bottle's Steam library)
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
replace_ini=false
[[ "${1:-}" == "--replace-ini" ]] && replace_ini=true

bottle="${CROSSOVER_BOTTLE:-Steam}"
game_dir="${GAME_DIR:-$HOME/Library/Application Support/CrossOver/Bottles/$bottle/drive_c/Program Files (x86)/Steam/steamapps/common/Enderal Special Edition}"
source_plugins="$repo_root/build/package/stage/SKSE/Plugins"
target_plugins="$game_dir/Data/SKSE/Plugins"

if [[ ! -f "$source_plugins/PiPPlayer.dll" ]]; then
	echo "Nothing staged. Run scripts/package.sh first." >&2
	exit 1
fi
if [[ ! -d "$target_plugins" ]]; then
	echo "$target_plugins not found (is SKSE installed, and is GAME_DIR right?)" >&2
	exit 1
fi

mkdir -p "$target_plugins/PiPPlayer"
cp "$source_plugins/PiPPlayer.dll" "$target_plugins/PiPPlayer.dll"
cp "$source_plugins/PiPPlayer/"* "$target_plugins/PiPPlayer/"
if $replace_ini || [[ ! -f "$target_plugins/PiPPlayer.ini" ]]; then
	cp "$source_plugins/PiPPlayer.ini" "$target_plugins/PiPPlayer.ini"
else
	echo "Kept existing PiPPlayer.ini (pass --replace-ini to overwrite)"
fi

echo "Deployed to $target_plugins"
