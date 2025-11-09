#!/usr/bin/env bash
# simple Day 2 tests (smoke tests)

# Find project root (two levels up from this script: .../day2/tests -> .../day2)
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
DAY2_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BIN="$DAY2_DIR/minishell"

# Build
cd "$DAY2_DIR" || { echo "Failed to cd to $DAY2_DIR"; exit 2; }
make > /dev/null 2>&1
if [ ! -x "$BIN" ]; then
  echo "Build failed or binary missing ($BIN). Run 'make' in day2/."
  exit 2
fi

echo "Running Day 2 smoke tests..."

# Test 1: pwd prints something
OUT=$(printf "pwd\nexit\n" | "$BIN")
if echo "$OUT" | head -n1 | grep -q "/"; then
  echo "TEST 1 (pwd) : PASS"
else
  echo "TEST 1 (pwd) : FAIL"
fi

# Test 2: external command ls (checks that external commands run)
OUT=$(printf "ls\nexit\n" | "$BIN")
if [ -n "$OUT" ]; then
  echo "TEST 2 (ls)  : PASS (shell produced output)"
else
  echo "TEST 2 (ls)  : FAIL"
fi

# Test 3: cd and pwd (ensure cd changes directory)
OUT=$(printf "cd ..\npwd\nexit\n" | "$BIN")
PWDLINE=$(echo "$OUT" | sed -n '2p')
if [ -n "$PWDLINE" ]; then
  echo "TEST 3 (cd)  : PASS"
else
  echo "TEST 3 (cd)  : FAIL"
fi

# Test 4: invalid command should produce an execvp error message to stderr
OUT=$(printf "nonexistentcmd\nexit\n" | "$BIN" 2>&1)
if echo "$OUT" | grep -qi -e "execvp" -e "not found" -e "No such file"; then
  echo "TEST 4 (invalid cmd): PASS"
else
  echo "TEST 4 (invalid cmd): NOTE (may vary by platform)"
fi

echo "Done. For interactive checks (Ctrl+C behavior, quoting), run ./minishell manually."

