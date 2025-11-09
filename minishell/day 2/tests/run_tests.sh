#!/bin/bash


echo "🧪 Starting automated minishell tests..."
echo "-----------------------------------------"

# Test 1: Print current directory
echo "▶️ Test 1: pwd"
pwd
echo "-----------------------------------------"

# Test 2: List current directory contents
echo "▶️ Test 2: ls"
ls
echo "-----------------------------------------"

# Test 3: Change directory and verify
echo "▶️ Test 3: cd and pwd"
cd ..
pwd
echo "-----------------------------------------"

# Test 4: Echo a message
echo "▶️ Test 4: echo command"
echo Hello from inside minishell!
echo "-----------------------------------------"

# Test 5: Invalid command
echo "▶️ Test 5: invalid command"
thiscommanddoesnotexist
echo "-----------------------------------------"

# Test 6: Check multiple commands in a row
echo "▶️ Test 6: sequence test"
pwd
ls
echo Done running multiple commands!
echo "-----------------------------------------"

# Test 7: Exit shell
echo "▶️ Test 7: exit minishell"
exit

