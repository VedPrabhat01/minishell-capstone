#!/usr/bin/env bash
# Run this from inside minishell:
# minishell> ./tests/run_bg_tests.sh

echo "=== Day 3 background/fg/bg/jobs test script ==="

echo "1) Start a background sleep"
sleep 2 &

echo "2) Start another background sleep"
sleep 4 &

echo "3) List jobs"
jobs

echo "4) Start a foreground sleep (press Ctrl+Z to suspend if you want to test stopping)"
sleep 3

echo "5) List jobs after foreground run (if any stopped)"
jobs

echo "6) Start a sleep and then stop it (Ctrl+Z simulation is manual); this script cannot send Ctrl+Z,
   but you can run 'sleep 30' manually and press Ctrl+Z, then 'jobs' to see it stopped."

echo "7) If you have a job id from jobs, try fg %1 (manually) to bring it to foreground."
echo "8) Try bg %1 to continue job 1 in background."

echo "9) Exiting the shell now (this script will call exit)."
exit

