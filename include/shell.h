#ifndef SHELL_H
#define SHELL_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

#define MAX_LEN 512
#define MAXARGS 64
#define ARGLEN 30
#define PROMPT "FCIT> "
#define HISTORY_SIZE 100

// Function prototypes
char* read_cmd(char* prompt, FILE* fp);
char** tokenize(char* cmdline);
int execute(char** arglist, int background, const char* cmdtext);
int handle_builtin(char** arglist);
int has_operator(char** arglist);
void add_to_history(const char* cmdline);
void print_history(void);
const char* get_history(int n);

void add_job(pid_t pid, const char* cmd);
void reap_jobs(void);
void print_jobs(void);
#endif // SHELL_H
