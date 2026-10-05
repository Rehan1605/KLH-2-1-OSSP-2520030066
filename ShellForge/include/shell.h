#ifndef SHELL_H
#define SHELL_H

#include <stdbool.h>
#include <stddef.h>
#include <signal.h>
#include <sys/types.h>

#include "history.h"

#define SHELL_MAX_JOBS 128

typedef enum
{
    JOB_RUNNING = 0,
    JOB_STOPPED = 1,
    JOB_DONE = 2
} JobState;

typedef struct
{
    pid_t pid;
    JobState state;
} JobProcess;

typedef struct
{
    int id;
    pid_t pgid;
    pid_t leader;
    char *name;
    JobState state;
    int status;
    bool notified;
    JobProcess *processes;
    size_t process_count;
} Job;

typedef struct
{
    Job jobs[SHELL_MAX_JOBS];
    int count;
    int next_id;
} JobTable;

typedef enum
{
    REDIR_INPUT,
    REDIR_OUTPUT,
    REDIR_APPEND,
    REDIR_ERROR,
    REDIR_ERROR_APPEND,
    REDIR_ERROR_TO_OUTPUT
} RedirectionType;

typedef struct
{
    RedirectionType type;
    char *target;
} Redirection;

typedef struct
{
    char **argv;
    int argc;
    Redirection *redirections;
    size_t redirection_count;
    size_t redirection_capacity;
    bool background;
} Command;

typedef struct
{
    Command *commands;
    int count;
    int capacity;
    bool background;
} Pipeline;

typedef struct
{
    bool interactive;
    pid_t shell_pid;
    pid_t shell_pgid;
    int terminal_fd;
    int last_status;
    bool exit_requested;
    char *cwd;
    char *prev_dir;
    char **env;
    int env_count;
    History history;
    JobTable jobs;
    int foreground_job_id;
    pid_t foreground_pgid;
} Shell;

void shell_init(Shell *shell);
void shell_free(Shell *shell);
void shell_update_cwd(Shell *shell);
char *shell_getenv(const Shell *shell, const char *key);
void shell_setenv(Shell *shell, const char *key, const char *value);
char *shell_find_executable(const Shell *shell, const char *name);
int shell_builtin_dispatch(Shell *shell, Command *command);
int shell_is_builtin(const char *name);
int shell_execute_line(Shell *shell, char *line);

void shell_print_prompt(const Shell *shell);
void shell_signal_setup(void);
void shell_block_jobcontrol_signals(sigset_t *oldmask);
void shell_restore_jobcontrol_signals(const sigset_t *oldmask);
void shell_reap_jobs(Shell *shell);

void job_table_init(JobTable *table);
void job_table_free(JobTable *table);
int job_table_add(JobTable *table, pid_t pgid, const pid_t *pids, size_t process_count, const char *name);
void job_table_remove_done(JobTable *table);
void job_table_update_status(JobTable *table, pid_t pid, int status);
void job_table_print(const JobTable *table);
int job_table_find_by_id(const JobTable *table, int id);
void job_table_reap_all(Shell *shell);
int job_table_resume_fg(Shell *shell, int job_id);

void pipeline_init(Pipeline *pipeline);
void pipeline_free(Pipeline *pipeline);
void pipeline_add_command(Pipeline *pipeline, Command *command);
void pipeline_reset_command(Command *command);
void pipeline_print(const Pipeline *pipeline);

int execute_pipeline(Shell *shell, Pipeline *pipeline);
int execute_builtin_with_redirections(Shell *shell, Command *command);

int builtin_cd(Shell *shell, Command *command);
int builtin_pwd(Shell *shell, Command *command);
int builtin_exit(Shell *shell, Command *command);
int builtin_export(Shell *shell, Command *command);
int builtin_history(Shell *shell, Command *command);
int builtin_jobs(Shell *shell, Command *command);
int builtin_fg(Shell *shell, Command *command);

#endif
