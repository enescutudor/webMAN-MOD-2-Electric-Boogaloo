#!/usr/bin/env bash
# Extrage din sursa webMAN definițiile funcțiilor `static` cerute, ca să fie testate pe PC exact așa cum sunt.
#   extract.sh <fișier> <funcție>...      scrie definițiile pe stdout
# Definiția începe la linia `static ... nume(` și se termină la primul `}` din coloana 0.
set -euo pipefail
export LC_ALL=C   # unele headere sunt ISO-8859

file=$1; shift
for fn in "$@"; do
	out=$(awk -v fn="$fn" '
		!on && $0 ~ "^static[^(]*[ *]" fn "\\(" && $0 !~ /;[ \t]*$/ { on = 1 }
		on { print }
		on && /^}/ { exit }
	' "$file")
	[[ -n $out ]] || { echo "extract.sh: $fn nu e în $file" >&2; exit 1; }
	printf '// --- %s: %s ---\n%s\n\n' "$file" "$fn" "$out"
done
