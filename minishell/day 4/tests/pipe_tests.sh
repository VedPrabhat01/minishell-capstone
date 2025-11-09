#!/usr/bin/env bash
# Run INSIDE minishell:
# minishell> ./tests/run_pipe_tests.sh

echo "=== Day 4 pipe & redirection tests ==="

echo "1) Simple pipeline: echo -e 'a\nb\nc' | grep b"
echo -e "a\nb\nc" | grep b

echo "---------------------------------"

echo "2) Pipeline with multiple stages: echo list | tr | wc"
echo "apple banana cherry" | tr ' ' '\n' | wc -l

echo "---------------------------------"

echo "3) Output redirection: echo Hello > tmp_out.txt then display with cat"
echo Hello Day4 > tmp_out.txt
cat tmp_out.txt

echo "---------------------------------"

echo "4) Input redirection: cat < tmp_out.txt"
cat < tmp_out.txt

echo "---------------------------------"

echo "5) Combined pipeline & redirection: echo -e '1\n2\n3\n4' | head -n 2 > tmp_head.txt; cat tmp_head.txt"
echo -e "1\n2\n3\n4" | head -n 2 > tmp_head.txt
cat tmp_head.txt

echo "---------------------------------"

echo "6) Background pipeline: sleep 2 | (sleep stage) &"
sleep 2 | sleep 1 &               # demonstrates pipeline backgrounding
jobs

echo "---------------------------------"

echo "7) Clean up and exit"
rm -f tmp_out.txt tmp_head.txt
exit

