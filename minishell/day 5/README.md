# minishell — Day 5

## Added features
- Persistent history: `~/.minishell_history`
- `kill` builtin: supports `kill <pid>`, `kill -9 <pid>`, `kill %<jobid>`, `kill -9 %<jobid>`
- `disown %<jobid>`: remove a job from job list
- Slightly nicer `jobs` display with timestamps
- History saved on exit

## Build
make

## Run
./minishell

