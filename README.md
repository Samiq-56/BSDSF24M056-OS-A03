# BSDSF24M056-OS-A03: A Shell in C

A simple UNIX shell (prompt `FCIT> `) built step by step for Operating Systems Assignment 03.

## Features

- v1.0-base: base shell that runs external commands with fork, execvp and waitpid
- v2: built-in commands `exit`, `cd`, `help`, `jobs`
- v3: command history with the `history` command and `!n` re-execution
- v4: GNU Readline integration (Tab completion, Up/Down arrow history)
- v5: I/O redirection (`<`, `>`) and pipes (`|`)
- v6: command chaining with `;`, background execution with `&`, working `jobs` list
- v7: multi-line `if` / `then` / `else` / `fi` based on command exit status
- v8: shell variables (`NAME=value`), `$NAME` expansion and the `set` command

## Build and Run

The Readline development library is required:

    sudo apt install libreadline-dev
    make
    ./bin/myshell

## Project Structure

    src/main.c       main loop, history, if/then/else/fi parsing
    src/shell.c      tokenizer, built-in commands, jobs, variables
    src/execute.c    fork/exec, redirection and pipes
    include/shell.h  shared declarations
    Makefile         build rules (links with -lreadline)

## Example Session

    FCIT> MSG="Hello, World"
    FCIT> echo $MSG
    Hello, World
    FCIT> ls | wc -l
    FCIT> sleep 5 &
    FCIT> jobs
    FCIT> exit
