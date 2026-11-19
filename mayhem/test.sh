#!/usr/bin/env bash
#
# mayhem/test.sh — run dust3d's functional self-test (built by mayhem/build.sh with normal
# flags) on the sample models upstream ships in application/resources, and diff every output
# line against the known-good values in mayhem/test/expected.txt: snapshot node/edge/part
# counts, generated mesh vertex/triangle counts and bounding box, the .ds3 write->read round
# trip (item names, sizes, checksums) and the XML save->load round trip.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${SRC:=/mayhem}"
cd "$SRC"

emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}

BIN="$SRC/mayhem/build/dust3d_selftest"
EXPECTED="$SRC/mayhem/test/expected.txt"
if [ ! -x "$BIN" ]; then
  echo "missing $BIN — run mayhem/build.sh first" >&2
  emit_ctrf dust3d-selftest 0 1
  exit 1
fi

passed=0 failed=0
# expected.txt: blocks of "== <mode> <file>" followed by the exact expected output lines.
cases="$(grep '^== ' "$EXPECTED" | sed 's/^== //')"
while read -r mode file; do
  [ -n "$mode" ] || continue
  want="$(awk -v hdr="== $mode $file" '$0==hdr{on=1;next} /^== /{on=0} on' "$EXPECTED")"
  got="$("$BIN" "$mode" "$file" 2>/dev/null)"
  if [ -n "$want" ] && [ "$got" = "$want" ]; then
    echo "PASS $mode $file"; passed=$((passed+1))
  else
    echo "FAIL $mode $file"; diff <(echo "$want") <(echo "$got") | head -20
    failed=$((failed+1))
  fi
done <<< "$cases"

emit_ctrf dust3d-selftest "$passed" "$failed"
