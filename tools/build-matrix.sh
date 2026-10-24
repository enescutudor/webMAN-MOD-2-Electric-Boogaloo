#!/usr/bin/env bash
# Compilează edițiile principale (flags/flags_*.h) în copii temporare ale arborelui de lucru,
# fără să modifice flags.h din repo. Rezultatul: <dir>/webftp_server_<ediție>.sprx
#
#   tools/build-matrix.sh [dir-ieșire] [ediții...]     (implicit: build/matrix, toate cele 4)
#
# Cere mediul Cell SDK (CELL_SDK, ppu-lv2-* și scetool în PATH).
set -euo pipefail

root=$(git rev-parse --show-toplevel)
out=$(realpath -m "${1:-$root/build/matrix}")
shift || true
if (($#)); then editions=("$@"); else editions=(full lite noncobra rebug_cobra_ps3mapi); fi
mkdir -p "$out"

: "${CELL_SDK:?CELL_SDK nu e setat}"
command -v scetool >/dev/null || { echo "scetool lipsește din PATH" >&2; exit 1; }

status=0
for ed in "${editions[@]}"; do
	tmp=$(mktemp -d)
	(cd "$root" && git ls-files -co --exclude-standard -z | tar -c --null -T - | tar -x -C "$tmp")
	cp "$tmp/flags/flags_$ed.h" "$tmp/flags.h"
	if (cd "$tmp" && make > "$out/build_$ed.log" 2>&1); then
		cp "$tmp/webftp_server.sprx" "$out/webftp_server_$ed.sprx"
		printf '%-22s OK   %8d octeți  %d avertismente\n' "$ed" \
			"$(stat -c %s "$out/webftp_server_$ed.sprx")" "$(grep -c 'warning:' "$out/build_$ed.log" || true)"
	else
		printf '%-22s EȘEC (vezi %s)\n' "$ed" "$out/build_$ed.log"
		status=1
	fi
	rm -rf "$tmp"
done
exit $status
