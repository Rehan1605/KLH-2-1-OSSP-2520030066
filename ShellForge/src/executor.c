#define _POSIX_C_SOURCE 200809L

#include "shell.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static void close_pipe_fds(int *fds, int count)
{
    for (int i = 0; i < count; i++)
    {
        if (fds[i] >= 0)
        {
            close(fds[i]);
            fds[i] = -1;
        }
    }
}

static int apply_redirections(const Command *command)
{
    if (command == NULL)
        return 0;

    for (size_t i = 0; i < command->redirection_count; i++)
    {
        const Redirection *redirection = &command->redirections[i];
        if (redirection->type == REDIR_ERROR_TO_OUTPUT)
        {
            if (dup2(STDOUT_FILENO, STDERR_FILENO) < 0)
            {
                perror("dup2");
                return 1;
            }
            continue;
        }

        int flags = O_WRONLY | O_CREAT;
        int destination = STDOUT_FILENO;
        mode_t create_mode = 0644;
        switch (redirection->type)
        {
            case REDIR_INPUT:
                flags = O_RDONLY;
                destination = STDIN_FILENO;
                break;
            case REDIR_OUTPUT:
                flags |= O_TRUNC;
                break;
            case REDIR_APPEND:
                flags |= O_APPEND;
                break;
            case REDIR_ERROR:
                flags |= O_TRUNC;
                destination = STDERR_FILENO;
                break;
            case REDIR_ERROR_APPEND:
                flags |= O_APPEND;
                destination = STDERR_FILENO;
                break;
            case REDIR_ERROR_TO_OUTPUT:
                continue;
        }

        int fd = open(redirection->target, flags, create_mode);
        if (fd < 0)
        {
            perror(redirection->target);
            return 1;
        }
        if (dup2(fd, destination) < 0)
        {
            perror("dup2");
            close(fd);
            return 1;
        }
        close(fd);
    }

    return 0;
}

static void child_exit_with_cleanup(
    Shell *shell,
    Pipeline *pipeline,
    pid_t *pids,
    int *pipes,
    int pipe_count,
    int status)
{
    if (pipes != NULL)
    {
        close_pipe_fds(pipes, pipe_count);
        free(pipes);
    }
    free(pids);

    for (int i = 0; i < shell->jobs.count; i++)
    {
        free(shell->jobs.jobs[i].name);
        free(shell->jobs.jobs[i].processes);
    }
    shell->jobs.count = 0;

    pipeline_free(pipeline);
    shell_free(shell);
    _exit(status);
}

int execute_builtin_with_redirections(Shell *shell, Command *command)
{
    int saved[3] = {-1, -1, -1};
    fflush(NULL);
    for (int i = 0; i < 3; i++)
    {
        saved[i] = dup(i);
        if (saved[i] < 0)
        {
            perror("dup");
            for (int j = 0; j < i; j++)
                close(saved[j]);
            return 1;
        }
    }

    int status = 1;
    if (apply_redirections(command) == 0)
        status = shell_builtin_dispatch(shell, command);
    fflush(NULL);

    for (int i = 0; i < 3; i++)
    {
        if (dup2(saved[i], i) < 0)
        {
            perror("dup2");
            status = 1;
        }
        close(saved[i]);
    }
    return status;
}

int execute_pipeline(Shell *shell, Pipeline *pipeline)
{
    if (shell == NULL || pipeline == NULL || pipeline->count <= 0)
        return 1;

    int command_count = pipeline->count;
    int pipe_count = command_count > 1 ? (command_count - 1) * 2 : 0;
    int *pipes = NULL;
    int launch_gate[2] = {-1, -1};
    bool use_launch_gate = shell->interactive && !pipeline->background;
    pid_t *pids = calloc((size_t)command_count, sizeof(pid_t));
    pid_t pipeline_pgid = -1;

    if (pids == NULL)
    {
        perror("calloc");
        return 1;
    }

    if (use_launch_gate && pipe(launch_gate) != 0)
    {
        perror("pipe");
        free(pids);
        return 1;
    }

    if (pipe_count > 0)
    {
        pipes = calloc((size_t)pipe_count, sizeof(int));
        if (pipes == NULL)
        {
            perror("calloc");
            free(pids);
            return 1;
        }

        for (int i = 0; i < command_count - 1; i++)
        {
            if (pipe(&pipes[i * 2]) != 0)
            {
                perror("pipe");
                close_pipe_fds(pipes, i * 2);
                if (launch_gate[0] >= 0)
                    close(launch_gate[0]);
                if (launch_gate[1] >= 0)
                    close(launch_gate[1]);
                free(pipes);
                free(pids);
                return 1;
            }
        }
    }

    char command_name[256] = {0};
    for (int i = 0; i < command_count; i++)
    {
        Command *command = &pipeline->commands[i];
        if (command->argv == NULL || command->argv[0] == NULL)
            continue;

        if (command_name[0] == '\0')
            snprintf(command_name, sizeof(command_name), "%s", command->argv[0]);

        pid_t pid = fork();
        if (pid < 0)
        {
            perror("fork");
            if (pipes != NULL)
                close_pipe_fds(pipes, pipe_count);
            if (pipeline_pgid > 0)
                kill(-pipeline_pgid, SIGKILL);
            if (launch_gate[0] >= 0)
                close(launch_gate[0]);
            if (launch_gate[1] >= 0)
                close(launch_gate[1]);
            for (int child = 0; child < i; child++)
            {
                int status;
                pid_t waited;
                do
                {
                    waited = waitpid(pids[child], &status, 0);
                } while (waited < 0 && errno == EINTR);
            }
            free(pipes);
            free(pids);
            return 1;
        }

        if (pid == 0)
        {
            pid_t child_pgid = pipeline_pgid == (pid_t)-1 ? 0 : pipeline_pgid;
            if (setpgid(0, child_pgid) != 0)
            {
                perror("setpgid");
                child_exit_with_cleanup(shell, pipeline, pids, pipes, pipe_count, 126);
            }

            struct sigaction default_action = {0};
            default_action.sa_handler = SIG_DFL;
            sigemptyset(&default_action.sa_mask);
            default_action.sa_flags = 0;
            sigaction(SIGINT, &default_action, NULL);
            sigaction(SIGQUIT, &default_action, NULL);
            sigaction(SIGTSTP, &default_action, NULL);
            sigaction(SIGTTIN, &default_action, NULL);
            sigaction(SIGTTOU, &default_action, NULL);
            sigaction(SIGCHLD, &default_action, NULL);

            if (use_launch_gate)
            {
                close(launch_gate[1]);
                char gate_byte;
                ssize_t gate_result;
                do
                {
                    gate_result = read(launch_gate[0], &gate_byte, 1);
                } while (gate_result < 0 && errno == EINTR);
                close(launch_gate[0]);
                if (gate_result < 0)
                {
                    perror("read(launch gate)");
                    child_exit_with_cleanup(shell, pipeline, pids, pipes, pipe_count, 126);
                }
            }

            if (command_count > 1)
            {
                if (i > 0)
                {
                    if (dup2(pipes[(i - 1) * 2], STDIN_FILENO) < 0)
                    {
                        perror("dup2");
                        child_exit_with_cleanup(shell, pipeline, pids, pipes, pipe_count, 126);
                    }
                }
                if (i < command_count - 1)
                {
                    if (dup2(pipes[i * 2 + 1], STDOUT_FILENO) < 0)
                    {
                        perror("dup2");
                        child_exit_with_cleanup(shell, pipeline, pids, pipes, pipe_count, 126);
                    }
                }
            }

            if (pipes != NULL)
                close_pipe_fds(pipes, pipe_count);

            if (apply_redirections(command) != 0)
                child_exit_with_cleanup(shell, pipeline, pids, pipes, pipe_count, 1);

            if (shell_is_builtin(command->argv[0]))
            {
                int status = shell_builtin_dispatch(shell, command);
                fflush(NULL);
                child_exit_with_cleanup(shell, pipeline, pids, pipes, pipe_count, status);
            }

            char *resolved = shell_find_executable(shell, command->argv[0]);
            if (resolved == NULL)
            {
                fprintf(stderr, "ShellForge: %s: command not found\n", command->argv[0]);
                child_exit_with_cleanup(shell, pipeline, pids, pipes, pipe_count, 127);
            }

            execve(resolved, command->argv, shell->env);
            perror(command->argv[0]);
            free(resolved);
            child_exit_with_cleanup(shell, pipeline, pids, pipes, pipe_count, 127);
        }

        pids[i] = pid;
        if (pipeline_pgid == (pid_t)-1)
        {
            pipeline_pgid = pid;
            if (setpgid(pid, pipeline_pgid) != 0 && errno != EACCES && errno != ESRCH)
                perror("setpgid");
        }
        else if (setpgid(pid, pipeline_pgid) != 0 && errno != EACCES && errno != ESRCH)
        {
            perror("setpgid");
        }

        if (pipes != NULL)
        {
            if (i > 0)
            {
                close(pipes[(i - 1) * 2]);
                pipes[(i - 1) * 2] = -1;
            }
            if (i < command_count - 1)
            {
                close(pipes[i * 2 + 1]);
                pipes[i * 2 + 1] = -1;
            }
        }
    }

    if (launch_gate[0] >= 0)
    {
        close(launch_gate[0]);
        launch_gate[0] = -1;
    }

    shell->foreground_pgid = pipeline_pgid;

    if (pipes != NULL)
    {
        free(pipes);
        pipes = NULL;
    }

    int job_id = job_table_add(&shell->jobs, pipeline_pgid, pids, (size_t)command_count, command_name);
    if (job_id < 0)
    {
        fprintf(stderr, "ShellForge: job table is full or allocation failed\n");
        kill(-pipeline_pgid, SIGKILL);
        if (launch_gate[0] >= 0)
            close(launch_gate[0]);
        if (launch_gate[1] >= 0)
            close(launch_gate[1]);
        if (pipes != NULL)
            close_pipe_fds(pipes, pipe_count);
        for (int i = 0; i < command_count; i++)
        {
            if (pids[i] <= 0)
                continue;
            int status;
            pid_t waited;
            do
            {
                waited = waitpid(pids[i], &status, 0);
            } while (waited < 0 && errno == EINTR);
        }
        free(pipes);
        free(pids);
        return 1;
    }
    if (job_id >= 0 && pipeline->background)
        printf("[%d] %ld\n", job_id, (long)pipeline_pgid);

    if (pipeline->background)
    {
        free(pids);
        return 0;
    }

    if (shell->interactive)
    {
        sigset_t oldmask;
        shell_block_jobcontrol_signals(&oldmask);
        if (tcsetpgrp(shell->terminal_fd, pipeline_pgid) != 0)
            perror("tcsetpgrp");
        shell_restore_jobcontrol_signals(&oldmask);
    }

    if (launch_gate[1] >= 0)
    {
        close(launch_gate[1]);
        launch_gate[1] = -1;
    }

    int exit_status = 0;
    bool stopped = false;
    int job_index = job_table_find_by_id(&shell->jobs, job_id);
    Job *job = job_index >= 0 ? &shell->jobs.jobs[job_index] : NULL;
    while (job != NULL && job->state == JOB_RUNNING)
    {
        int status = 0;
        pid_t waited = 0;
        while (true)
        {
            waited = waitpid(-pipeline_pgid, &status, WUNTRACED | WCONTINUED);
            if (waited < 0)
            {
                if (errno == EINTR)
                    continue;
                if (errno == ECHILD)
                    break;
                perror("waitpid");
                break;
            }
            break;
        }

        if (waited <= 0)
            break;

        job_table_update_status(&shell->jobs, waited, status);

        if (waited == pids[command_count - 1] && WIFEXITED(status))
            exit_status = WEXITSTATUS(status);
        else if (waited == pids[command_count - 1] && WIFSIGNALED(status))
            exit_status = 128 + WTERMSIG(status);
        else if (WIFSTOPPED(status))
        {
            exit_status = 128 + WSTOPSIG(status);
            stopped = true;
        }
    }

    if (stopped && exit_status == 0)
        exit_status = 128 + SIGTSTP;
    job_table_remove_done(&shell->jobs);

    if (shell->interactive)
    {
        sigset_t oldmask;
        shell_block_jobcontrol_signals(&oldmask);
        if (tcsetpgrp(shell->terminal_fd, shell->shell_pgid) != 0)
            perror("tcsetpgrp");
        shell_restore_jobcontrol_signals(&oldmask);
    }

    shell->foreground_pgid = shell->shell_pgid;
    shell->foreground_job_id = 0;
    shell->last_status = exit_status;
    free(pids);
    return exit_status;
}
