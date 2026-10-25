#!/usr/bin/env bash
# Runs each test in its own process.
#   run.sh <binary> <known-bugs.txt>
# A test listed in known-bugs.txt must fail (XFAIL); if it passes, the bug is fixed and it must be removed from the list.
set -uo pipefail

bin=$1 known=$2
fail=0 pass=0 xfail=0
for t in $("$bin" --list); do
	expected_fail=0
	grep -qxF "$t" <(grep -v '^#' "$known") && expected_fail=1
	if out=$("$bin" "$t" 2>&1); then
		if ((expected_fail)); then echo "XPASS $t  (fixed? remove it from $known)"; fail=$((fail + 1))
		else echo "ok    $t"; pass=$((pass + 1)); fi
	else
		if ((expected_fail)); then
			echo "xfail $t  ($(grep -m1 -oE 'ERROR: AddressSanitizer: [a-z-]+|CHECK\(.*\)|runtime error: .*' <<< "$out"))"
			xfail=$((xfail + 1))
		else echo "FAIL  $t"; sed 's/^/      /' <<< "$out" | head -20; fail=$((fail + 1)); fi
	fi
done
echo "--- $pass ok, $xfail xfail (known bugs), $fail failures"
((fail == 0))
