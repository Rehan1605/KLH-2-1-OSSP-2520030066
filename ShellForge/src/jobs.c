#define _POSIX_C_SOURCE 200809L

#include "shell.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

void job_table_init(JobTable *table)
{
    if (table == NULL)
        return;

    memset(table, 0, sizeof(*table));
    table->count = 0;
    table->next_id = 1;
}

void job_table_free(JobTable *table)
{
    if (table == NULL)
        return;

    for (int i = 0; i < table->count; i++)
    {
        kill(-table->jobs[i].pgid, SIGHUP);
        if (table->jobs[i].state == JOB_STOPPED)
            kill(-table->jobs[i].pgid, SIGCONT);
        free(table->jobs[i].name);
        free(table->jobs[i].processes);
    }
    memset(table, 0, sizeof(*table));
    table->count = 0;
    table->next_id = 1;
}

int job_table_add(JobTable *table, pid_t pgid, const pid_t *pids, size_t process_count, const char *name)
{
    if (table == NULL || name == NULL || pids == NULL || process_count == 0)
        return -1;

    if (table->count >= SHELL_MAX_JOBS)
        return -1;

    Job *job = &table->jobs[table->count];
    JobProcess *processes = calloc(process_count, sizeof(*processes));
    char *job_name = strdup(name);
    if (processes == NULL || job_name == NULL)
    {
        perror("job allocation");
        free(processes);
        free(job_name);
        return -1;
    }
    for (size_t i = 0; i < process_count; i++)
    {
        processes[i].pid = pids[i];
        processes[i].state = JOB_RUNNING;
    }
    job->id = table->next_id++;
    job->pgid = pgid;
    job->leader = pids[0];
    job->name = job_name;
    job->state = JOB_RUNNING;
    job->status = 0;
    job->notified = false;
    job->processes = processes;
    job->process_count = process_count;
    table->count++;
    return job->id;
}

void job_table_remove_done(JobTable *table)
{
    if (table == NULL)
        return;

    int write_index = 0;
    for (int i = 0; i < table->count; i++)
    {
        if (table->jobs[i].state != JOB_DONE)
        {
            if (write_index != i)
                table->jobs[write_index] = table->jobs[i];
            write_index++;
        }
        else
        {
            free(table->jobs[i].name);
            free(table->jobs[i].processes);
        }
    }
    table->count = write_index;
}

void job_table_update_status(JobTable *table, pid_t pid, int status)
{
    if (table == NULL)
        return;

    for (int i = 0; i < table->count; i++)
    {
        Job *job = &table->jobs[i];
        for (size_t process_index = 0; process_index < job->process_count; process_index++)
        {
            JobProcess *process = &job->processes[process_index];
            if (process->pid != pid)
                continue;

            if (WIFEXITED(status))
            {
                process->state = JOB_DONE;
                job->status = WEXITSTATUS(status);
            }
            else if (WIFSTOPPED(status))
            {
                process->state = JOB_STOPPED;
                job->status = WSTOPSIG(status);
            }
            else if (WIFSIGNALED(status))
            {
                process->state = JOB_DONE;
                job->status = WTERMSIG(status);
            }
            else if (WIFCONTINUED(status))
            {
                process->state = JOB_RUNNING;
                job->status = 0;
            }

            bool all_done = true;
            bool all_inactive = true;
            bool any_stopped = false;
            for (size_t member = 0; member < job->process_count; member++)
            {
                JobState member_state = job->processes[member].state;
                if (member_state != JOB_DONE)
                    all_done = false;
                if (member_state == JOB_RUNNING)
                    all_inactive = false;
                if (member_state == JOB_STOPPED)
                    any_stopped = true;
            }
            job->state = all_done ? JOB_DONE :
                         (all_inactive && any_stopped ? JOB_STOPPED : JOB_RUNNING);
            break;
        }
    }
}

void job_table_print(const JobTable *table)
{
    if (table == NULL)
        return;

    for (int i = 0; i < table->count; i++)
    {
        const Job *job = &table->jobs[i];
        const char *state = job->state == JOB_RUNNING ? "Running" :
                            (job->state == JOB_STOPPED ? "Stopped" : "Done");
        printf("[%d] %ld %s %s\n", job->id, (long)job->pgid, state, job->name);
    }
}

int job_table_find_by_id(const JobTable *table, int id)
{
    if (table == NULL)
        return -1;

    for (int i = 0; i < table->count; i++)
    {
        if (table->jobs[i].id == id)
            return i;
    }

    return -1;
}

void job_table_reap_all(Shell *shell)
{
    if (shell == NULL)
        return;

    int status = 0;
    pid_t pid;
    for (;;)
    {
        pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED);
        if (pid <= 0)
        {
            if (pid < 0 && errno == EINTR)
                continue;
            break;
        }
        job_table_update_status(&shell->jobs, pid, status);
    }

    job_table_remove_done(&shell->jobs);
}

int job_table_resume_fg(Shell *shell, int job_id)
{
    if (shell == NULL)
        return 1;

    int index = job_table_find_by_id(&shell->jobs, job_id);
    if (index < 0)
    {
        fprintf(stderr, "fg: no such job\n");
        return 1;
    }

    Job *job = &shell->jobs.jobs[index];
    pid_t pgid = job->pgid;
    int job_status = job->status;

    sigset_t oldmask;
    shell_block_jobcontrol_signals(&oldmask);
    if (shell->interactive && tcsetpgrp(shell->terminal_fd, pgid) != 0)
    {
        shell_restore_jobcontrol_signals(&oldmask);
        perror("tcsetpgrp");
        return 1;
    }
    shell_restore_jobcontrol_signals(&oldmask);

    shell->foreground_job_id = job->id;
    shell->foreground_pgid = pgid;
    if (job->state == JOB_STOPPED)
    {
        if (kill(-pgid, SIGCONT) != 0)
            perror("kill(SIGCONT)");
        for (size_t i = 0; i < job->process_count; i++)
            if (job->processes[i].state == JOB_STOPPED)
                job->processes[i].state = JOB_RUNNING;
        job->state = JOB_RUNNING;
    }

    while (job->state == JOB_RUNNING)
    {
        int status = 0;
        pid_t waited = waitpid(-pgid, &status, WUNTRACED | WCONTINUED);
        if (waited < 0)
        {
            if (errno == EINTR)
                continue;
            if (errno == ECHILD)
            {
                for (size_t i = 0; i < job->process_count; i++)
                    job->processes[i].state = JOB_DONE;
                job->state = JOB_DONE;
            }
            else
                perror("waitpid");
            break;
        }
        job_table_update_status(&shell->jobs, waited, status);
        if (waited == job->leader && (WIFEXITED(status) || WIFSIGNALED(status)))
            job_status = WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
        if (WIFSTOPPED(status))
            job_status = 128 + WSTOPSIG(status);
    }

    shell_block_jobcontrol_signals(&oldmask);
    if (shell->interactive && tcsetpgrp(shell->terminal_fd, shell->shell_pgid) != 0)
        perror("tcsetpgrp");
    shell_restore_jobcontrol_signals(&oldmask);
    shell->foreground_pgid = shell->shell_pgid;
    shell->foreground_job_id = 0;
    shell->last_status = job_status;
    if (job->state == JOB_DONE)
        job_table_remove_done(&shell->jobs);
    return job_status;
}
