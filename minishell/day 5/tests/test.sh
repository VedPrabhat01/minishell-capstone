#!/usr/bin/env bash
echo "=== Day 5 quick tests (run INSIDE minishell) ==="

echo "1) Start a background sleep"
sleep 10 &

echo "2) jobs (should show the sleep job)"
jobs

echo "3) kill the job by job id (replace %1 if different)"
# This line is demonstration only; can't rely on a fixed id. Print message for manual step.
echo "Manually run: kill %1   (or kill <pid>)"

echo "4) Test disown: start sleep 20 & then disown %2 (or appropriate id)"
echo "Manual steps: sleep 20 &   then 'jobs' to get id then 'disown %<id>'"

echo "5) Test persistent history: run 'history' then exit the shell and restart shell; history should contain recent commands."

echo "6) Example: run 'echo hello', then 'history' to see it listed"
echo "echo hello"
echo hello

echo "7) Please try the following manually:"
echo "   - Start a background sleep (sleep 30 &)."
echo "   - Run 'jobs' to find its %id."
echo "   - Run 'kill -9 %<id>' to force-stop it."
echo "   - Start another background job and run 'disown %<id>' and verify it disappears from 'jobs'."

echo "8) Exiting test script (no 'exit' command here)."

