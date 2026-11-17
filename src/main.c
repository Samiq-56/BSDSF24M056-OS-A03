#include "shell.h"
#include <ctype.h>
#include <readline/readline.h>
#include <readline/history.h>

// Run one command (tokens cmd[0..len-1], NULL terminated) in foreground or background
static void run_command(char** cmd, int len, int background) {
    if (len == 0) return;

    // Text version of the command, used by the jobs list
    char text[MAX_LEN];
    text[0] = '\0';
    for (int i = 0; i < len; i++) {
        if (i > 0) strncat(text, " ", MAX_LEN - strlen(text) - 1);
        strncat(text, cmd[i], MAX_LEN - strlen(text) - 1);
    }

    if (has_operator(cmd) || handle_builtin(cmd) == 0) {
        execute(cmd, background, text);
    }
}

int main() {
    char* cmdline;
    char** arglist;

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

        if ((arglist = tokenize(cmdline)) != NULL) {
            // Split the tokens at ';' and '&' and run each command in order
            char* cmd[MAXARGS + 1];
            int len = 0;

            for (int i = 0; arglist[i] != NULL; i++) {
                if (strcmp(arglist[i], ";") == 0 || strcmp(arglist[i], "&") == 0) {
                    int background = (strcmp(arglist[i], "&") == 0);
                    cmd[len] = NULL;
                    run_command(cmd, len, background);
                    len = 0;
                } else {
                    cmd[len++] = arglist[i];
                }
            }
            cmd[len] = NULL;
            run_command(cmd, len, 0);

            // Free the memory allocated by tokenize()
            for (int i = 0; arglist[i] != NULL; i++) {
                free(arglist[i]);
            }
            free(arglist);
        }
        free(cmdline);
    }

    printf("\nShell exited.\n");
    return 0;
}
