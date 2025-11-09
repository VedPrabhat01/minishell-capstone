# minishell — Day 4

## Goal
Add piping and redirection:
- `command1 | command2 | command3`
- `command < infile`
- `command > outfile`
- Combine with background `&` and existing job control

## Build
make

## Run
./minishell

## Examples
- `ls -l | grep .cpp | wc -l`
- `sort < unsorted.txt > sorted.txt`
- `cat file | grep foo > out.txt &`
- `sleep 10 &` then `jobs` then `fg %1`

