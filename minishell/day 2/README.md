# minishell — Day 2

## Goal
Implement execution of basic commands through the shell.

## Features (Day 2)
- Execute external commands using `fork()` + `execvp()`
- Parent shell ignores `Ctrl+C`; child processes get default behavior
- Built-ins: `cd`, `pwd`, `exit`, `history`
- Simple quoted-argument parsing (supports single and double quotes)

## Build
```bash
make

