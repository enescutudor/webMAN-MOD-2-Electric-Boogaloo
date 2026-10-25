#!/usr/bin/env bash
# Checks whether the fork touches the red zone (PROTECTED_PATHS.txt) or the amber zone (tools/amber-paths.txt).
#
#   tools/check-protected.sh                  diff against upstream (BASE, default upstream/master...HEAD)
#   tools/check-protected.sh --staged         files in the index (pre-commit hook)
#   tools/check-protected.sh --msg <file>     require "Impact low-level" in the message if the index touches the amber zone (commit-msg hook)
#
# Red zone: exit 1. Amber zone: warning only (or exit 1 with --msg if the section is missing).
# Merges from upstream pass: they bring upstream changes unmodified.
set -euo pipefail

root=$(git rev-parse --show-toplevel)
red_list="$root/PROTECTED_PATHS.txt"
amber_list="$root/tools/amber-paths.txt"

# Prints the paths from stdin that match an entry of list $1 (exact path, or prefix ending in /).
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
	*) echo "usage: $0 [--staged | --msg <file>]" >&2; exit 2 ;;
esac

# 7b.4: the PS3MAPI_REENABLE_SYSCALL8 call in www_page.h stays unchanged, on the same line as upstream
page=include/www/www_page.h
syscall8() { LC_ALL=C grep -n 'PS3MAPI_REENABLE_SYSCALL8'; }
if [[ $mode == diff ]]; then now=$(syscall8 < "$root/$page"); else now=$(git show ":$page" | syscall8); fi
if [[ $now != "$(git show "$(git merge-base upstream/master HEAD):$page" | syscall8)" ]]; then
	echo "ERROR: $page: the PS3MAPI_REENABLE_SYSCALL8 line differs from upstream (7b.4)" >&2
	exit 1
fi

red=$(match "$red_list" <<< "$files")
amber=$(match "$amber_list" <<< "$files")
amber=$(grep -vxF -f <(printf '%s\n' "$red") <<< "$amber" || true)

if [[ $mode == --msg ]]; then
	if [[ -n $amber ]] && ! grep -qi 'impact low-level' "$2"; then
		echo "ERROR: the commit touches the amber zone and the message has no \"Impact low-level\" section:" >&2
		sed 's/^/  /' <<< "$amber" >&2
		exit 1
	fi
	exit 0
fi

if [[ -n $amber ]]; then
	echo "WARNING: amber zone (requires \"Impact low-level\" in the commit message):" >&2
	sed 's/^/  /' <<< "$amber" >&2
fi
if [[ -n $red ]]; then
	echo "ERROR: red zone, protected files (PROTECTED_PATHS.txt):" >&2
	sed 's/^/  /' <<< "$red" >&2
	exit 1
fi
exit 0
