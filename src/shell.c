#include "shell.h"
#include <ctype.h>

char* read_cmd(char* prompt, FILE* fp) {
    printf("%s", prompt);
    char* cmdline = (char*) malloc(sizeof(char) * MAX_LEN);
    int c, pos = 0;

    while ((c = getc(fp)) != EOF) {
        if (c == '\n') break;
        cmdline[pos++] = c;
    }

    if (c == EOF && pos == 0) {
        free(cmdline);
        return NULL; // Handle Ctrl+D
    }
    
    cmdline[pos] = '\0';
    return cmdline;
}

char** tokenize(char* cmdline) {
    if (cmdline == NULL || cmdline[0] == '\0') {
        return NULL;
    }

    char** arglist = (char**)calloc(MAXARGS + 1, sizeof(char*));
    int argnum = 0;
    char* cp = cmdline;

    while (*cp != '\0' && argnum < MAXARGS) {
        while (*cp == ' ' || *cp == '\t') cp++;   // skip whitespace
        if (*cp == '\0') break;

        char* tok = (char*)malloc(strlen(cp) + 1);
        int len = 0;

        if (*cp == '<' || *cp == '>' || *cp == '|' || *cp == ';' || *cp == '&') {
            // Operators are always a token of their own
            tok[len++] = *cp++;
        } else {
            while (*cp != '\0' && *cp != ' ' && *cp != '\t' &&
                   *cp != '<' && *cp != '>' && *cp != '|' && *cp != ';' && *cp != '&') {
                if (*cp == '"' || *cp == '\'') {
                    // Quoted text: copy it without the quotes
                    char quote = *cp++;
                    while (*cp != '\0' && *cp != quote) {
                        tok[len++] = *cp++;
                    }
                    if (*cp == quote) cp++;
                } else {
                    tok[len++] = *cp++;
                }
            }
        }
        tok[len] = '\0';
        arglist[argnum++] = tok;
    }

    if (argnum == 0) {
        free(arglist);
        return NULL;
    }

    arglist[argnum] = NULL;
    return arglist;
}

// Returns 1 if the command was a built-in (and handled), 0 otherwise
int handle_builtin(char** arglist) {
    if (arglist == NULL || arglist[0] == NULL) {
        return 0;
    }

    // exit
    if (strcmp(arglist[0], "exit") == 0) {
        printf("Exiting shell...\n");
        exit(0);
    }

    // cd <directory>
    if (strcmp(arglist[0], "cd") == 0) {
        const char* target = arglist[1];

        // "cd" with no argument goes to HOME
        if (target == NULL || target[0] == '\0') {
            target = getenv("HOME");
            if (target == NULL) {
                fprintf(stderr, "cd: HOME not set\n");
                return 1;
            }
        }

        if (chdir(target) != 0) {
            perror("cd failed");
        }
        return 1;
    }

    // help
    if (strcmp(arglist[0], "help") == 0) {
        printf("Built-in commands:\n");
        printf("  exit        - Terminate the shell\n");
        printf("  cd <dir>    - Change the current working directory\n");
        printf("  help        - Show this help message\n");
        printf("  history     - Show command history (use !n to re-run)\n");
        printf("  set         - Show shell variables (define with NAME=value)\n");
        printf("  jobs        - List running background jobs\n");
        return 1;
    }

    // set
    if (strcmp(arglist[0], "set") == 0) {
        print_vars();
        return 1;
    }

    // history
    if (strcmp(arglist[0], "history") == 0) {
        print_history();
        return 1;
    }

    // jobs (placeholder)
    if (strcmp(arglist[0], "jobs") == 0) {
        print_jobs();
        return 1;
    }

    return 0; // Not a built-in
}


// ---------- Command History ----------
static char* history[HISTORY_SIZE];
static int history_count = 0;

// Store a command. If full, drop the oldest one.
void add_to_history(const char* cmdline) {
    if (history_count == HISTORY_SIZE) {
        free(history[0]);
        for (int i = 1; i < HISTORY_SIZE; i++) {
            history[i - 1] = history[i];
        }
        history_count--;
    }
    history[history_count++] = strdup(cmdline);
}

// Print all stored commands with line numbers starting from 1
void print_history(void) {
    for (int i = 0; i < history_count; i++) {
        printf("%4d  %s\n", i + 1, history[i]);
    }
}

// Return the n-th command (1-based), or NULL if n is out of range
const char* get_history(int n) {
    if (n < 1 || n > history_count) {
        return NULL;
    }
    return history[n - 1];
}


// ---------- Background Jobs ----------
#define MAX_JOBS 64

typedef struct {
    int id;
    pid_t pid;
    char cmd[MAX_LEN];
} job_t;

static job_t jobs_list[MAX_JOBS];
static int job_count = 0;
static int next_job_id = 1;

// Remember a background process
void add_job(pid_t pid, const char* cmd) {
    if (job_count == MAX_JOBS) {
        fprintf(stderr, "Too many background jobs\n");
        return;
    }
    jobs_list[job_count].id = next_job_id++;
    jobs_list[job_count].pid = pid;
    strncpy(jobs_list[job_count].cmd, cmd, MAX_LEN - 1);
    jobs_list[job_count].cmd[MAX_LEN - 1] = '\0';
    printf("[%d] %d\n", jobs_list[job_count].id, (int)pid);
    job_count++;
}

// Collect finished children (non-blocking) so they do not stay zombies
void reap_jobs(void) {
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        for (int i = 0; i < job_count; i++) {
            if (jobs_list[i].pid == pid) {
                printf("[%d]  Done    %s\n", jobs_list[i].id, jobs_list[i].cmd);
                for (int j = i + 1; j < job_count; j++) {
                    jobs_list[j - 1] = jobs_list[j];
                }
                job_count--;
                break;
            }
        }
    }
}

// Print all running background jobs
void print_jobs(void) {
    if (job_count == 0) {
        printf("No background jobs.\n");
        return;
    }
    for (int i = 0; i < job_count; i++) {
        printf("[%d]  %d  Running  %s\n", jobs_list[i].id, (int)jobs_list[i].pid, jobs_list[i].cmd);
    }
}


// ---------- Shell Variables ----------
typedef struct var {
    char* name;
    char* value;
    struct var* next;
} var_t;

static var_t* var_head = NULL;

// Create a variable, or change its value if it already exists
void set_var(const char* name, const char* value) {
    var_t* last = NULL;

    for (var_t* v = var_head; v != NULL; v = v->next) {
        if (strcmp(v->name, name) == 0) {
            free(v->value);
            v->value = strdup(value);
            return;
        }
        last = v;
    }

    var_t* node = (var_t*)malloc(sizeof(var_t));
    node->name = strdup(name);
    node->value = strdup(value);
    node->next = NULL;

    if (last == NULL) {
        var_head = node;
    } else {
        last->next = node;
    }
}

// Look up a shell variable, then fall back to the environment (HOME, USER, ...)
const char* get_var(const char* name) {
    for (var_t* v = var_head; v != NULL; v = v->next) {
        if (strcmp(v->name, name) == 0) {
            return v->value;
        }
    }
    return getenv(name);
}

// Print all shell variables
void print_vars(void) {
    if (var_head == NULL) {
        printf("No variables defined.\n");
        return;
    }
    for (var_t* v = var_head; v != NULL; v = v->next) {
        printf("%s=%s\n", v->name, v->value);
    }
}

// 1 if the token looks like NAME=value (valid NAME, no spaces before the =)
int is_assignment(const char* token) {
    const char* eq = strchr(token, '=');

    if (eq == NULL || eq == token) return 0;
    if (isalpha((unsigned char)token[0]) == 0 && token[0] != '_') return 0;

    for (const char* p = token; p < eq; p++) {
        if (isalnum((unsigned char)*p) == 0 && *p != '_') return 0;
    }
    return 1;
}

// If every token is NAME=value, store them all and return 1. Otherwise return 0.
int try_assignment(char** cmd, int len) {
    for (int i = 0; i < len; i++) {
        if (is_assignment(cmd[i]) == 0) return 0;
    }

    for (int i = 0; i < len; i++) {
        char* eq = strchr(cmd[i], '=');
        *eq = '\0';
        set_var(cmd[i], eq + 1);
        *eq = '=';
    }
    return 1;
}

// Returns a NEW string where every $NAME in the word is replaced by its value.
// An undefined variable becomes an empty string. The caller must free() the result.
char* expand_word(const char* word) {
    size_t cap = strlen(word) + 1;
    char* out = (char*)malloc(cap);
    size_t len = 0;
    const char* p = word;

    while (*p != '\0') {
        if (*p == '$' && (isalpha((unsigned char)p[1]) || p[1] == '_')) {
            char name[MAX_LEN];
            int n = 0;

            p++;   // skip the '$'
            while ((isalnum((unsigned char)*p) || *p == '_') && n < MAX_LEN - 1) {
                name[n++] = *p++;
            }
            name[n] = '\0';

            const char* val = get_var(name);
            if (val == NULL) val = "";

            size_t vl = strlen(val);
            while (len + vl + 1 > cap) {
                cap *= 2;
                out = (char*)realloc(out, cap);
            }
            memcpy(out + len, val, vl);
            len += vl;
        } else {
            if (len + 2 > cap) {
                cap *= 2;
                out = (char*)realloc(out, cap);
            }
            out[len++] = *p++;
        }
    }

    out[len] = '\0';
    return out;
}
