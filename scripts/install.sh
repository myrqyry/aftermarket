#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "$0")/.." && pwd)"
build="$(realpath -m "$root/build")"
if (( $# )); then build="$1"; fi
src="$build/aftermarket.so"
[[ -s "$src" ]] || { echo "Build the plugin first: $src" >&2; exit 1; }
helper="$build/aftermarket-restart-helper"
[[ -x "$helper" ]] || { echo "Build the restart helper first: $helper" >&2; exit 1; }
config="${XDG_CONFIG_HOME:-$HOME/.config}/obs-studio"
plugins="$config/plugins"
dest="$plugins/aftermarket/bin/64bit"
legacy="$plugins/obs-ui-augment"
rollback="${XDG_DATA_HOME:-$HOME/.local/share}/aftermarket/rollback"
mkdir -p "$dest"
temp="$(mktemp "$dest/.aftermarket.XXXXXX")"
backup=""
published=false
cleanup(){
  rm -f -- "$temp"
  if [[ "$published" == false && -n "$backup" && -d "$backup/obs-ui-augment" && ! -e "$legacy" ]]; then
    mv -- "$backup/obs-ui-augment" "$legacy"
  fi
}
trap cleanup EXIT
install -m 755 "$src" "$temp"
if [[ -e "$legacy" || -L "$legacy" ]]; then
  [[ -d "$legacy" && ! -L "$legacy" ]] || { echo "Resolve legacy plugin path manually: $legacy" >&2; exit 1; }
  mkdir -p "$rollback"
  backup="$(mktemp -d "$rollback/pre-identity.XXXXXX")"
  # Disable the entire old package outside OBS's plugin search path.
  # Never overwrite the mapped binary of a running OBS instance.
  mv -- "$legacy" "$backup/obs-ui-augment"
  echo "Previous plugin preserved at $backup/obs-ui-augment"
fi
install -Dm644 "$root/LICENSE" "$plugins/aftermarket/data/LICENSE"
install -Dm644 "$root/third_party/libebur128/THIRD_PARTY_NOTICES.txt" "$plugins/aftermarket/data/THIRD_PARTY_NOTICES.txt"
install -m 755 "$helper" "$dest/.aftermarket-restart-helper.new"
mv -f -- "$dest/.aftermarket-restart-helper.new" "$dest/aftermarket-restart-helper"
mv -f -- "$temp" "$dest/aftermarket.so"
published=true
echo "Installed $dest/aftermarket.so"
echo "Restart OBS before opening another instance. Legacy settings migrate at startup."
