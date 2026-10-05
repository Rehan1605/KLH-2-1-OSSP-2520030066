#define _POSIX_C_SOURCE 200809L

#include "shell.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int shell_is_builtin(const char *name)
{
    static const char *builtins[] = {
        "cd", "pwd", "exit", "export", "history", "jobs", "fg", NULL
    };

    if (name == NULL)
        return 0;

    for (int i = 0; builtins[i] != NULL; i++)
    {
        if (strcmp(name, builtins[i]) == 0)
            return 1;
    }

    return 0;
}

static int get_arg_count(Command *command)
{
    return command == NULL ? 0 : command->argc;
}

int builtin_cd(Shell *shell, Command *command)
{
    if (shell == NULL || command == NULL)
        return 1;

    if (command->argc > 2)
    {
        fprintf(stderr, "cd: too many arguments\n");
        return 1;
    }

    const char *target = NULL;
    if (get_arg_count(command) == 1)
        target = shell_getenv(shell, "HOME");
    else if (get_arg_count(command) >= 2)
        target = command->argv[1];

    if (target == NULL)
    {
        fprintf(stderr, "cd: missing target\n");
        return 1;
    }

    bool print_new_directory = strcmp(target, "-") == 0;
    if (print_new_directory)
    {
        if (shell->prev_dir == NULL || shell->prev_dir[0] == '\0')
        {
            fprintf(stderr, "cd: no previous directory\n");
            return 1;
        }
        target = shell->prev_dir;
    }

    char *oldcwd = getcwd(NULL, 0);
    if (oldcwd == NULL)
    {
        perror("getcwd");
        return 1;
    }

    if (chdir(target) != 0)
    {
        perror("chdir");
        free(oldcwd);
        return 1;
    }

    char *newcwd = getcwd(NULL, 0);
    if (newcwd == NULL)
    {
        perror("getcwd");
        if (chdir(oldcwd) != 0)
            perror("chdir(restore)");
        free(oldcwd);
        return 1;
    }

    shell_setenv(shell, "OLDPWD", oldcwd);
    shell_setenv(shell, "PWD", newcwd);
    if (shell->prev_dir != NULL)
        free(shell->prev_dir);
    free(shell->cwd);
    shell->prev_dir = oldcwd;
    shell->cwd = newcwd;
    if (print_new_directory)
        printf("%s\n", shell->cwd);

    return 0;
}

int builtin_pwd(Shell *shell, Command *command)
{
    (void)command;
    if (shell == NULL)
        return 1;

    if (shell->cwd == NULL)
        shell_update_cwd(shell);

    if (shell->cwd != NULL)
        printf("%s\n", shell->cwd);
    else
        perror("getcwd");

    return 0;
}

int builtin_exit(Shell *shell, Command *command)
{
    (void)command;
    if (shell == NULL)
        return 0;

    shell->exit_requested = true;
    return 0;
}

int builtin_export(Shell *shell, Command *command)
{
    if (shell == NULL || command == NULL)
        return 1;

    for (int i = 1; i < command->argc; i++)
    {
        char *eq = strchr(command->argv[i], '=');
        if (eq == NULL)
        {
            char *value = shell_getenv(shell, command->argv[i]);
            if (value == NULL)
                continue;
            printf("%s=%s\n", command->argv[i], value);
            continue;
        }

        *eq = '\0';
        shell_setenv(shell, command->argv[i], eq + 1);
        *eq = '=';
    }

    return 0;
}

int builtin_history(Shell *shell, Command *command)
{
    (void)command;
    if (shell == NULL)
        return 1;

    history_print(&shell->history);
    return 0;
}

int builtin_jobs(Shell *shell, Command *command)
{
    (void)command;
    if (shell == NULL)
        return 1;

    job_table_print(&shell->jobs);
    return 0;
}

int builtin_fg(Shell *shell, Command *command)
{
    if (shell == NULL)
        return 1;

    if (command->argc > 2)
    {
        fprintf(stderr, "fg: too many arguments\n");
        return 1;
    }

    int id = -1;
    if (command->argc > 1)
    {
        const char *job_spec = command->argv[1];
        if (*job_spec == '%')
            job_spec++;
        char *end = NULL;
        errno = 0;
        long parsed_id = strtol(job_spec, &end, 10);
        if (job_spec[0] == '\0' || *end != '\0' || errno != 0 ||
            parsed_id <= 0 || parsed_id > INT_MAX)
        {
            fprintf(stderr, "fg: invalid job id\n");
            return 1;
        }
        id = (int)parsed_id;
    }
    else if (shell->jobs.count > 0)
        id = shell->jobs.jobs[shell->jobs.count - 1].id;

    if (id < 0)
    {
        fprintf(stderr, "fg: no job selected\n");
        return 1;
    }

    return job_table_resume_fg(shell, id);
}

int shell_builtin_dispatch(Shell *shell, Command *command)
{
    if (shell == NULL || command == NULL || command->argv == NULL || command->argv[0] == NULL)
        return 1;

    if (strcmp(command->argv[0], "cd") == 0)
        return builtin_cd(shell, command);
    if (strcmp(command->argv[0], "pwd") == 0)
        return builtin_pwd(shell, command);
    if (strcmp(command->argv[0], "exit") == 0)
        return builtin_exit(shell, command);
    if (strcmp(command->argv[0], "export") == 0)
        return builtin_export(shell, command);
    if (strcmp(command->argv[0], "history") == 0)
        return builtin_history(shell, command);
    if (strcmp(command->argv[0], "jobs") == 0)
        return builtin_jobs(shell, command);
    if (strcmp(command->argv[0], "fg") == 0)
        return builtin_fg(shell, command);

    return 127;
}
