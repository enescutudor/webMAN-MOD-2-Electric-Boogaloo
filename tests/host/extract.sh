#!/usr/bin/env bash
# Extracts the requested `static` function definitions from the webMAN sources, so they are tested on the PC exactly as they are.
#   extract.sh <file> <function>...      writes the definitions to stdout
# A definition starts at the `static ... name(` line and ends at the first `}` in column 0.
set -euo pipefail
export LC_ALL=C   # some headers are ISO-8859

file=$1; shift
for fn in "$@"; do
	out=$(awk -v fn="$fn" '
		!on && $0 ~ "^static[^(]*[ *]" fn "\\(" && $0 !~ /;[ \t]*$/ { on = 1 }
		on { print }
		on && /^}/ { exit }
	' "$file")
	[[ -n $out ]] || { echo "extract.sh: $fn not found in $file" >&2; exit 1; }
	printf '// --- %s: %s ---\n%s\n\n' "$file" "$fn" "$out"
done
