#!/usr/bin/env bash
# Verifică dacă fork-ul atinge zona roșie (PROTECTED_PATHS.txt) sau galbenă (tools/amber-paths.txt).
#
#   tools/check-protected.sh                  diff față de upstream (BASE, implicit upstream/master...HEAD)
#   tools/check-protected.sh --staged         fișierele din index (hook-ul pre-commit)
#   tools/check-protected.sh --msg <fișier>   cere „Impact low-level” în mesaj dacă indexul atinge zona galbenă (hook-ul commit-msg)
#
# Zona roșie: exit 1. Zona galbenă: doar avertisment (sau exit 1 la --msg, dacă lipsește secțiunea).
# Merge-urile din upstream trec: aduc modificările lor neschimbate.
set -euo pipefail

root=$(git rev-parse --show-toplevel)
red_list="$root/PROTECTED_PATHS.txt"
amber_list="$root/tools/amber-paths.txt"

# Afișează căile din stdin care se potrivesc cu o intrare din lista $1 (cale exactă sau prefix terminat în /).
match() {
	local list=$1 path entry
	while IFS= read -r path; do
		while IFS= read -r entry; do
			[[ -z $entry || $entry == \#* ]] && continue
			if [[ $path == "$entry" || ($entry == */ && $path == "$entry"*) ]]; then
				echo "$path"
				break
			fi
		done < "$list"
	done
}

mode=${1:-diff}
if [[ $mode != diff && -f "$(git rev-parse --git-dir)/MERGE_HEAD" ]]; then
	exit 0
fi

case $mode in
	diff)    files=$(git diff --name-only "${BASE:-upstream/master...HEAD}") ;;
	--staged|--msg) files=$(git diff --cached --name-only) ;;
	*) echo "folosire: $0 [--staged | --msg <fișier>]" >&2; exit 2 ;;
esac

red=$(match "$red_list" <<< "$files")
amber=$(match "$amber_list" <<< "$files")
amber=$(grep -vxF -f <(printf '%s\n' "$red") <<< "$amber" || true)

if [[ $mode == --msg ]]; then
	if [[ -n $amber ]] && ! grep -qi 'impact low-level' "$2"; then
		echo "EROARE: commit-ul atinge zona galbenă și mesajul nu are secțiunea „Impact low-level”:" >&2
		sed 's/^/  /' <<< "$amber" >&2
		exit 1
	fi
	exit 0
fi

if [[ -n $amber ]]; then
	echo "ATENȚIE: zona galbenă (cere „Impact low-level” în mesajul de commit):" >&2
	sed 's/^/  /' <<< "$amber" >&2
fi
if [[ -n $red ]]; then
	echo "EROARE: zona roșie, fișiere protejate (PROTECTED_PATHS.txt):" >&2
	sed 's/^/  /' <<< "$red" >&2
	exit 1
fi
exit 0
