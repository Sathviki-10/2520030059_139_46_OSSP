
#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."
BIN=./dining_os_lab
TMP=$(mktemp)
trap 'rm -f "$TMP"' EXIT

"$BIN" simulate --philosophers 5 --meals 2 --quiet --log "$TMP"
grep -q 'Total meals completed: 10' "$TMP"

"$BIN" simulate --philosophers 7 --meals 1 --strategy ordered --quiet --log "$TMP"
grep -q 'Total meals completed: 7' "$TMP"

"$BIN" help > "$TMP"
grep -q 'simulate' "$TMP"
grep -q -- '--philosophers N' "$TMP"

if grep -Eq 'process|ipc|memory|files|CO-[1-6]' "$TMP"; then
    echo "Help contains unrelated commands or CO labels."
    exit 1
fi

echo "All simulation smoke tests passed."

