#ifndef JOBS_H
#define JOBS_H

#include <sys/types.h>

#define MAX_JOBS 256
#define MAX_PROCS_PER_JOB 128

typedef enum {
    JOB_RUNNING,
    JOB_STOPPED
} JobState;

typedef struct {
    pid_t pid;
    char cmd_name[256];
    int exited;
} ProcessInfo;

typedef struct {
    int job_number;
    pid_t pgid;
    ProcessInfo procs[MAX_PROCS_PER_JOB];
    int proc_count;
    JobState state;
    int active;
    char command_line[1024];
} Job;

void init_all_signals();
void check_bg_jobs();
int add_job(pid_t pgid, ProcessInfo procs[], int proc_count, JobState state, const char *cmd_line);
Job* find_job(int job_number);
Job* find_job_by_pid(pid_t pid);
void mark_job_state(int job_number, JobState state);
void remove_job(int job_number);
void execute_activities();
int has_stopped_jobs();
int has_any_jobs();
void send_sighup_all();
int is_tracked_pid(pid_t pid);
int is_tracked_job(int job_number);
int was_sigint();

#endif
