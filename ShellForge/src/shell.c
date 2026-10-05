#define _POSIX_C_SOURCE 200809L

#include "shell.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "parser.h"

static void copy_env(char ***out_env, int *out_count)
{
    extern char **environ;
    int count = 0;

    while (environ[count] != NULL)
        count++;

    char **env = calloc((size_t)count + 1, sizeof(char *));
    if (env == NULL)
    {
        perror("calloc");
        *out_env = NULL;
        *out_count = 0;
        return;
    }

    for (int i = 0; i < count; i++)
    {
        env[i] = strdup(environ[i]);
        if (env[i] == NULL)
        {
            perror("strdup");
            for (int j = 0; j < i; j++)
                free(env[j]);
            free(env);
            *out_env = NULL;
            *out_count = 0;
            return;
        }
    }

    *out_env = env;
    *out_count = count;
}

static void free_env(char **env, int count)
{
    if (env == NULL)
        return;

    for (int i = 0; i < count; i++)
        free(env[i]);
    free(env);
}

void shell_init(Shell *shell)
{
    if (shell == NULL)
        return;

    memset(shell, 0, sizeof(*shell));

    shell->shell_pid = getpid();
    shell->terminal_fd = open("/dev/tty", O_RDWR | O_CLOEXEC);
    shell->interactive = isatty(STDIN_FILENO) && shell->terminal_fd >= 0;
    shell->shell_pgid = getpgrp();
    if (shell->interactive)
    {
        struct sigaction default_action = {0};
        default_action.sa_handler = SIG_DFL;
        sigemptyset(&default_action.sa_mask);
        sigaction(SIGTTIN, &default_action, NULL);

        for (;;)
        {
            pid_t terminal_pgid = tcgetpgrp(shell->terminal_fd);
            if (terminal_pgid < 0)
            {
                perror("tcgetpgrp");
                break;
            }
            shell->shell_pgid = getpgrp();
            if (terminal_pgid == shell->shell_pgid)
                break;
            if (kill(-shell->shell_pgid, SIGTTIN) != 0 && errno != EINTR)
            {
                perror("kill(SIGTTIN)");
                break;
            }
        }

        if (setpgid(0, 0) != 0 && getpgrp() != shell->shell_pid)
            perror("setpgid(shell)");
        shell->shell_pgid = getpgrp();

        sigset_t oldmask;
        shell_block_jobcontrol_signals(&oldmask);
        if (tcsetpgrp(shell->terminal_fd, shell->shell_pgid) != 0)
            perror("tcsetpgrp(shell)");
        shell_restore_jobcontrol_signals(&oldmask);
    }
    shell->foreground_pgid = shell->shell_pgid;
    shell->last_status = 0;
    shell->exit_requested = false;
    shell->cwd = getcwd(NULL, 0);
    shell->prev_dir = strdup(".");
    history_init(&shell->history);
    job_table_init(&shell->jobs);
    copy_env(&shell->env, &shell->env_count);
    shell_signal_setup();
}

void shell_free(Shell *shell)
{
    if (shell == NULL)
        return;

    history_free(&shell->history);
    job_table_free(&shell->jobs);
    free_env(shell->env, shell->env_count);
    free(shell->cwd);
    free(shell->prev_dir);
    if (shell->terminal_fd >= 0)
        close(shell->terminal_fd);
}

void shell_update_cwd(Shell *shell)
{
    char *newcwd = getcwd(NULL, 0);
    if (newcwd == NULL)
        return;

    if (shell->cwd != NULL)
        free(shell->cwd);
    shell->cwd = newcwd;
}

char *shell_getenv(const Shell *shell, const char *key)
{
    if (shell == NULL || key == NULL || shell->env == NULL)
        return NULL;

    size_t key_len = strlen(key);
    for (int i = 0; i < shell->env_count; i++)
    {
        if (strncmp(shell->env[i], key, key_len) == 0 &&
            shell->env[i][key_len] == '=')
        {
            return shell->env[i] + key_len + 1;
        }
    }
    return NULL;
}

void shell_setenv(Shell *shell, const char *key, const char *value)
{
    if (shell == NULL || key == NULL)
        return;

    size_t key_len = strlen(key);
    size_t val_len = value == NULL ? 0 : strlen(value);
    char *entry = malloc(key_len + val_len + 2);
    if (entry == NULL)
    {
        perror("malloc");
        return;
    }

    snprintf(entry, key_len + val_len + 2, "%s=%s", key, value == NULL ? "" : value);

    for (int i = 0; i < shell->env_count; i++)
    {
        if (strncmp(shell->env[i], key, key_len) == 0 &&
            shell->env[i][key_len] == '=')
        {
            free(shell->env[i]);
            shell->env[i] = entry;
            return;
        }
    }

    char **new_env = realloc(shell->env, (size_t)(shell->env_count + 2) * sizeof(char *));
    if (new_env == NULL)
    {
        perror("realloc");
        free(entry);
        return;
    }

    shell->env = new_env;
    shell->env[shell->env_count++] = entry;
    shell->env[shell->env_count] = NULL;
}

char *shell_find_executable(const Shell *shell, const char *name)
{
    if (shell == NULL || name == NULL || name[0] == '\0')
        return NULL;

    if (strchr(name, '/') != NULL)
    {
        return strdup(name);
    }

    const char *path = shell_getenv(shell, "PATH");
    if (path == NULL)
        return NULL;

    char *non_executable = NULL;
    const char *entry = path;
    for (;;)
    {
        const char *separator = strchr(entry, ':');
        size_t directory_length = separator == NULL ? strlen(entry) : (size_t)(separator - entry);
        const char *directory = directory_length == 0 ? "." : entry;
        if (directory_length == 0)
            directory_length = 1;
        size_t len = directory_length + strlen(name) + 2;
        char *candidate = malloc(len);
        if (candidate == NULL)
        {
            perror("malloc");
            free(non_executable);
            return NULL;
        }

        snprintf(candidate, len, "%.*s/%s", (int)directory_length, directory, name);
        if (access(candidate, X_OK) == 0)
        {
            free(non_executable);
            return candidate;
        }

        if (non_executable == NULL && access(candidate, F_OK) == 0)
            non_executable = candidate;
        else
            free(candidate);

        if (separator == NULL)
            break;
        entry = separator + 1;
    }

    return non_executable;
}

void shell_print_prompt(const Shell *shell)
{
    (void)shell;
    printf("shellforge$ ");
    fflush(stdout);
}

int shell_execute_line(Shell *shell, char *line)
{
    if (shell == NULL || line == NULL)
        return 1;

    const unsigned char *cursor = (const unsigned char *)line;
    while (*cursor != '\0' && isspace(*cursor))
        cursor++;
    if (*cursor == '\0')
    {
        free(line);
        return 0;
    }

    shell_reap_jobs(shell);
    history_add(&shell->history, line);

    Pipeline pipeline;
    pipeline_init(&pipeline);

    if (!parse_pipeline(line, shell, &pipeline))
    {
        pipeline_free(&pipeline);
        free(line);
        return 1;
    }

    free(line);

    if (pipeline.count == 0)
    {
        pipeline_free(&pipeline);
        return 1;
    }

    if (pipeline.background && pipeline.count == 1 && shell_is_builtin(pipeline.commands[0].argv[0]))
    {
        fprintf(stderr, "ShellForge: built-ins cannot run in the background\n");
        pipeline_free(&pipeline);
        return 1;
    }

    if (pipeline.count == 1 && !pipeline.background && shell_is_builtin(pipeline.commands[0].argv[0]))
    {
        int status = pipeline.commands[0].redirection_count == 0 ?
                     shell_builtin_dispatch(shell, &pipeline.commands[0]) :
                     execute_builtin_with_redirections(shell, &pipeline.commands[0]);
        shell->last_status = status;
        pipeline_free(&pipeline);
        return status;
    }

    int rc = execute_pipeline(shell, &pipeline);
    shell_reap_jobs(shell);

    pipeline_free(&pipeline);
    return rc;
}

void shell_reap_jobs(Shell *shell)
{
    if (shell == NULL)
        return;

    job_table_reap_all(shell);
}
