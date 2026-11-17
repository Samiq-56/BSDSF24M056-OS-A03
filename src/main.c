#include "shell.h"
#include <ctype.h>
#include <readline/readline.h>
#include <readline/history.h>

#define MAX_BLOCK 128

// ---------- small helpers for the if/then/else/fi parser ----------

static char* skip_ws(char* s) {
    while (*s == ' ' || *s == '\t') s++;
    return s;
}

// 1 if the line is exactly the given keyword (spaces around it are allowed)
static int is_keyword(char* line, const char* word) {
    line = skip_ws(line);
    size_t n = strlen(word);
    if (strncmp(line, word, n) != 0) return 0;
    return *skip_ws(line + n) == '\0';
}

// 1 if the line starts with the keyword "if"
static int is_if_line(char* line) {
    line = skip_ws(line);
    if (strncmp(line, "if", 2) != 0) return 0;
    return line[2] == ' ' || line[2] == '\t' || line[2] == '\0';
}

// ---------- running commands ----------

// Run one command (tokens cmd[0..len-1], NULL terminated). Returns its exit status.
static int run_command(char** cmd_in, int len, int background) {
    if (len == 0) return 0;

    // Expansion step: replace $VAR words with their values.
    // These are new strings, freed at the end of this function.
    char* cmd[MAXARGS + 1];
    for (int i = 0; i < len; i++) {
        cmd[i] = expand_word(cmd_in[i]);
    }
    cmd[len] = NULL;

    int status;

    if (try_assignment(cmd, len)) {
        // NAME=value is handled by the shell itself, no fork
        status = 0;
    } else {
        // Text version of the command, used by the jobs list
        char text[MAX_LEN];
        text[0] = '\0';
        for (int i = 0; i < len; i++) {
            if (i > 0) strncat(text, " ", MAX_LEN - strlen(text) - 1);
            strncat(text, cmd[i], MAX_LEN - strlen(text) - 1);
        }

        if (has_operator(cmd)) {
            status = execute(cmd, background, text);
        } else if (handle_builtin(cmd)) {
            status = 0;
        } else {
            status = execute(cmd, background, text);
        }
    }

    for (int i = 0; i < len; i++) {
        free(cmd[i]);
    }
    return status;
}

// Tokenize one line, run every command in it (split at ; and &)
// and return the exit status of the last command.
static int run_line(char* cmdline) {
    char** arglist = tokenize(cmdline);
    int status = 0;

    if (arglist == NULL) return 0;

    char* cmd[MAXARGS + 1];
    int len = 0;

    for (int i = 0; arglist[i] != NULL; i++) {
        if (strcmp(arglist[i], ";") == 0 || strcmp(arglist[i], "&") == 0) {
            int background = (strcmp(arglist[i], "&") == 0);
            cmd[len] = NULL;
            if (len > 0) status = run_command(cmd, len, background);
            len = 0;
        } else {
            cmd[len++] = arglist[i];
        }
    }
    cmd[len] = NULL;
    if (len > 0) status = run_command(cmd, len, 0);

    // Free the memory allocated by tokenize()
    for (int i = 0; arglist[i] != NULL; i++) {
        free(arglist[i]);
    }
    free(arglist);

    return status;
}

static int run_lines(char** lines, int n);

// lines[0] is "if <command>" and lines[n-1] is the matching "fi"
static int run_if(char** lines, int n) {
    char* cond = skip_ws(skip_ws(lines[0]) + 2);
    if (*cond == '\0') {
        fprintf(stderr, "syntax error: missing condition after 'if'\n");
        return 1;
    }

    char* then_lines[MAX_BLOCK];
    char* else_lines[MAX_BLOCK];
    int then_n = 0;
    int else_n = 0;
    int state = 0;   // 0 = waiting for then, 1 = then block, 2 = else block
    int depth = 0;   // depth of nested if statements

    for (int i = 1; i < n - 1; i++) {
        char* line = lines[i];

        if (is_if_line(line)) {
            depth++;
        } else if (is_keyword(line, "fi")) {
            depth--;
        }

        if (depth == 0 && state == 0 && is_keyword(line, "then")) {
            state = 1;
            continue;
        }
        if (depth == 0 && state == 1 && is_keyword(line, "else")) {
            state = 2;
            continue;
        }

        if (state == 0) {
            char* t = skip_ws(line);
            if (*t == '\0' || *t == '#') continue;
            fprintf(stderr, "syntax error: expected 'then'\n");
            return 1;
        }

        if (state == 1 && then_n < MAX_BLOCK) {
            then_lines[then_n++] = line;
        } else if (state == 2 && else_n < MAX_BLOCK) {
            else_lines[else_n++] = line;
        }
    }

    if (state == 0) {
        fprintf(stderr, "syntax error: missing 'then'\n");
        return 1;
    }

    // Run the condition and pick the block from its exit status
    int status = run_line(cond);
    if (status == 0) {
        return run_lines(then_lines, then_n);
    }
    if (else_n > 0) {
        return run_lines(else_lines, else_n);
    }
    return 0;
}

// Run a list of lines one after another (handles nested if blocks)
static int run_lines(char** lines, int n) {
    int status = 0;
    int i = 0;

    while (i < n) {
        char* line = lines[i];
        char* t = skip_ws(line);

        if (*t == '\0' || *t == '#') {
            i++;
            continue;
        }

        if (is_if_line(line)) {
            // find the matching fi
            int depth = 0;
            int end = -1;
            for (int j = i; j < n; j++) {
                if (is_if_line(lines[j])) {
                    depth++;
                } else if (is_keyword(lines[j], "fi")) {
                    depth--;
                    if (depth == 0) {
                        end = j;
                        break;
                    }
                }
            }
            if (end == -1) {
                fprintf(stderr, "syntax error: missing 'fi'\n");
                return 1;
            }
            status = run_if(&lines[i], end - i + 1);
            i = end + 1;
        } else if (is_keyword(line, "then") || is_keyword(line, "else") || is_keyword(line, "fi")) {
            fprintf(stderr, "syntax error: unexpected '%s'\n", t);
            return 1;
        } else {
            status = run_line(line);
            i++;
        }
    }
    return status;
}

// ---------- main loop ----------

int main() {
    char* cmdline;

    while (1) {
        // Collect finished background jobs before showing the prompt
        reap_jobs();

        cmdline = readline(PROMPT);
        if (cmdline == NULL) break;

        // Handle history re-execution (number after the exclamation mark) first
        if (cmdline[0] == '!') {
            char* num = cmdline + 1;
            int valid = (*num != '\0');
            for (char* q = num; *q != '\0'; q++) {
                if (!isdigit((unsigned char)*q)) {
                    valid = 0;
                    break;
                }
            }

            const char* old = valid ? get_history(atoi(num)) : NULL;
            if (old == NULL) {
                printf("%s: event not found\n", cmdline);
                free(cmdline);
                continue;
            }

            // Replace the history reference with the stored command
            char* expanded = strdup(old);
            free(cmdline);
            cmdline = expanded;
            printf("%s\n", cmdline);
        }

        // Save to history only if the line has some real text
        int has_text = 0;
        for (char* p = cmdline; *p != '\0'; p++) {
            if (*p != ' ' && *p != '\t') {
                has_text = 1;
                break;
            }
        }
        if (has_text) {
            add_history(cmdline);       // readline history (Up/Down arrows)
            add_to_history(cmdline);    // our own history (history command)
        }

        // An "if" line starts a multi-line block that ends with a matching "fi"
        if (is_if_line(cmdline)) {
            char* block[MAX_BLOCK];
            int count = 0;
            int depth = 0;
            int ok = 1;
            char* line = cmdline;

            while (1) {
                if (count == MAX_BLOCK) {
                    fprintf(stderr, "if block is too long\n");
                    free(line);
                    ok = 0;
                    break;
                }
                block[count++] = line;

                if (is_if_line(line)) {
                    depth++;
                } else if (is_keyword(line, "fi")) {
                    depth--;
                }
                if (depth == 0) break;

                line = readline("> ");
                if (line == NULL) {
                    fprintf(stderr, "syntax error: end of input, missing 'fi'\n");
                    ok = 0;
                    break;
                }
            }

            if (ok) run_lines(block, count);
            for (int i = 0; i < count; i++) free(block[i]);
            continue;
        }

        run_line(cmdline);
        free(cmdline);
    }

    printf("\nShell exited.\n");
    return 0;
}
