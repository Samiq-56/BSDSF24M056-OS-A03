#include "shell.h"
#include <fcntl.h>

static int is_operator(const char* s) {
    return strcmp(s, "|") == 0 || strcmp(s, "<") == 0 || strcmp(s, ">") == 0;
}

// Returns 1 if the token list contains any of < > |
int has_operator(char** arglist) {
    for (int i = 0; arglist[i] != NULL; i++) {
        if (is_operator(arglist[i])) return 1;
    }
    return 0;
}

// Runs one command (tokens start..end-1) inside a child process.
// Applies < and > redirection, then runs the built-in or execvp. Never returns.
static void run_segment(char** arglist, int start, int end) {
    char* argv[MAXARGS + 1];
    int argc = 0;
    char* infile = NULL;
    char* outfile = NULL;

    for (int i = start; i < end; i++) {
        if (strcmp(arglist[i], "<") == 0) {
            if (i + 1 >= end || is_operator(arglist[i + 1])) {
                fprintf(stderr, "syntax error: expected filename after '<'\n");
                exit(1);
            }
            infile = arglist[++i];
        } else if (strcmp(arglist[i], ">") == 0) {
            if (i + 1 >= end || is_operator(arglist[i + 1])) {
                fprintf(stderr, "syntax error: expected filename after '>'\n");
                exit(1);
            }
            outfile = arglist[++i];
        } else {
            argv[argc++] = arglist[i];
        }
    }
    argv[argc] = NULL;

    if (argc == 0) {
        fprintf(stderr, "syntax error: missing command\n");
        exit(1);
    }

    if (infile != NULL) {
        int fd = open(infile, O_RDONLY);
        if (fd < 0) {
            perror(infile);
            exit(1);
        }
        dup2(fd, STDIN_FILENO);
        close(fd);
    }

    if (outfile != NULL) {
        int fd = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) {
            perror(outfile);
            exit(1);
        }
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }

    // Built-ins (history, help, ...) can also be used inside a pipeline
    if (handle_builtin(argv)) {
        fflush(stdout);
        exit(0);
    }

    execvp(argv[0], argv);
    perror("Command not found");
    exit(1);
}

int execute(char* arglist[], int background, const char* cmdtext) {
    int starts[MAXARGS + 2];
    int ends[MAXARGS + 2];
    pid_t pids[MAXARGS + 2];
    int nseg = 1;
    int n = 0;

    while (arglist[n] != NULL) n++;

    // Split the token list into segments at every "|"
    starts[0] = 0;
    for (int i = 0; i < n; i++) {
        if (strcmp(arglist[i], "|") == 0) {
            ends[nseg - 1] = i;
            starts[nseg] = i + 1;
            nseg++;
        }
    }
    ends[nseg - 1] = n;

    // Every segment must contain something
    for (int s = 0; s < nseg; s++) {
        if (ends[s] <= starts[s]) {
            fprintf(stderr, "syntax error near '|'\n");
            return -1;
        }
    }

    int prev_read = -1;   // read end of the previous pipe
    int launched = 0;
    fflush(stdout);

    for (int s = 0; s < nseg; s++) {
        int pfd[2] = {-1, -1};

        if (s < nseg - 1) {
            if (pipe(pfd) < 0) {
                perror("pipe failed");
                break;
            }
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork failed");
            if (pfd[0] != -1) close(pfd[0]);
            if (pfd[1] != -1) close(pfd[1]);
            break;
        }

        if (pid == 0) {
            // Child: connect stdin to previous pipe, stdout to next pipe
            if (prev_read != -1) {
                dup2(prev_read, STDIN_FILENO);
                close(prev_read);
            }
            if (pfd[1] != -1) {
                dup2(pfd[1], STDOUT_FILENO);
                close(pfd[0]);
                close(pfd[1]);
            }
            run_segment(arglist, starts[s], ends[s]);
        }

        // Parent: close the ends it does not need, keep the read end for the next child
        pids[launched++] = pid;
        if (prev_read != -1) close(prev_read);
        if (pfd[1] != -1) close(pfd[1]);
        prev_read = pfd[0];
    }

    if (prev_read != -1) close(prev_read);

    if (background) {
        // Do not wait: remember the last process of this line as a background job
        if (launched > 0) {
            add_job(pids[launched - 1], cmdtext);
        }
        return 0;
    }

    for (int i = 0; i < launched; i++) {
        int status;
        waitpid(pids[i], &status, 0);
    }
    return 0;
}
