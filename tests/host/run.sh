#!/usr/bin/env bash
# Rulează fiecare test într-un proces separat.
#   run.sh <binar> <known-bugs.txt>
# Un test din known-bugs.txt trebuie să pice (XFAIL); dacă trece, bug-ul e reparat și se scoate din listă.
set -uo pipefail

bin=$1 known=$2
fail=0 pass=0 xfail=0
for t in $("$bin" --list); do
	expected_fail=0
	grep -qxF "$t" <(grep -v '^#' "$known") && expected_fail=1
	if out=$("$bin" "$t" 2>&1); then
		if ((expected_fail)); then echo "XPASS $t  (reparat? scoate-l din $known)"; fail=$((fail + 1))
		else echo "ok    $t"; pass=$((pass + 1)); fi
	else
		if ((expected_fail)); then
			echo "xfail $t  ($(grep -m1 -oE 'ERROR: AddressSanitizer: [a-z-]+|CHECK\(.*\)|runtime error: .*' <<< "$out"))"
			xfail=$((xfail + 1))
		else echo "FAIL  $t"; sed 's/^/      /' <<< "$out" | head -20; fail=$((fail + 1)); fi
	fi
done
echo "--- $pass ok, $xfail xfail (bug-uri cunoscute), $fail eșecuri"
((fail == 0))
