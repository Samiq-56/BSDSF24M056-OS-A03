#include "shell.h"
#include <ctype.h>
#include <readline/readline.h>
#include <readline/history.h>

int main() {
    char* cmdline;
    char** arglist;

    // readline() prints the prompt, handles editing, arrows and Tab completion
    while ((cmdline = readline(PROMPT)) != NULL) {

        // Handle !n before tokenizing or saving to history
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

            // Replace "!n" with the stored command
            char* expanded = strdup(old);
            free(cmdline);
            cmdline = expanded;
            printf("%s\n", cmdline);   // show which command is being re-run
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
            add_to_history(cmdline);    // our own history (history command, !n)
        }

        if ((arglist = tokenize(cmdline)) != NULL) {

            // Run external command only if it is NOT a built-in
            if (handle_builtin(arglist) == 0) {
                execute(arglist);
            }

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
