#!/usr/bin/env bash
# Builds the mount plugins netiso.sprx and raw_iso.sprx in temporary copies of the tree.
# Their makefiles call ../../scetool, so the copy gets a link to the scetool found in PATH.
#
#   tools/build-plugins.sh [output-dir]     (default: build/plugins)
#
# Requires the Cell SDK environment (CELL_SDK, ppu-lv2-* and scetool in PATH).
set -euo pipefail

root=$(git rev-parse --show-toplevel)
out=$(realpath -m "${1:-$root/build/plugins}")
mkdir -p "$out"

: "${CELL_SDK:?CELL_SDK is not set}"
scetool=$(command -v scetool) || { echo "scetool is missing from PATH" >&2; exit 1; }

tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
(cd "$root" && git ls-files -co --exclude-standard -z | tar -c --null -T - | tar -x -C "$tmp")
ln -sf "$scetool" "$tmp/scetool"

for dir in netiso rawseciso; do
	(cd "$tmp/_Projects_/$dir" && make > "$out/build_$dir.log" 2>&1) || { echo "$dir: FAILED (see $out/build_$dir.log)"; exit 1; }
	for f in "$tmp/_Projects_/$dir"/*.sprx; do
		cp "$f" "$out/"
		printf '%-14s OK  %6d bytes\n' "$(basename "$f")" "$(stat -c %s "$f")"
	done
done
