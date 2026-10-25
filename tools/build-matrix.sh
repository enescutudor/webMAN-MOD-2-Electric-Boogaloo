#!/usr/bin/env bash
# Builds the main editions (flags/flags_*.h) in temporary copies of the working tree,
# without modifying flags.h in the repo. Output: <dir>/webftp_server_<edition>.sprx
#
#   tools/build-matrix.sh [output-dir] [editions...]     (default: build/matrix, all 4)
#
# Requires the Cell SDK environment (CELL_SDK, ppu-lv2-* and scetool in PATH).
set -euo pipefail

root=$(git rev-parse --show-toplevel)
out=$(realpath -m "${1:-$root/build/matrix}")
shift || true
if (($#)); then editions=("$@"); else editions=(full lite noncobra rebug_cobra_ps3mapi); fi
mkdir -p "$out"

: "${CELL_SDK:?CELL_SDK is not set}"
command -v scetool >/dev/null || { echo "scetool is missing from PATH" >&2; exit 1; }

status=0
for ed in "${editions[@]}"; do
	tmp=$(mktemp -d)
	(cd "$root" && git ls-files -co --exclude-standard -z | tar -c --null -T - | tar -x -C "$tmp")
	cp "$tmp/flags/flags_$ed.h" "$tmp/flags.h"
	if (cd "$tmp" && make > "$out/build_$ed.log" 2>&1); then
		cp "$tmp/webftp_server.sprx" "$out/webftp_server_$ed.sprx"
		printf '%-22s OK     %8d bytes  %d warnings\n' "$ed" \
			"$(stat -c %s "$out/webftp_server_$ed.sprx")" "$(grep -c 'warning:' "$out/build_$ed.log" || true)"
	else
		printf '%-22s FAILED (see %s)\n' "$ed" "$out/build_$ed.log"
		status=1
	fi
	rm -rf "$tmp"
done
exit $status
