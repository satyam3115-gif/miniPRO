#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include "../include/jobs.h"

static Job jobs[MAX_JOBS];
static int job_count = 0;
static int next_job_number = 1;
static volatile sig_atomic_t sigchld_received = 0;
static volatile sig_atomic_t sigint_received = 0;

static void sigchld_handler(int sig) {
    (void)sig;
    sigchld_received = 1;
}

static void sigint_handler(int sig) {
    (void)sig;
    sigint_received = 1;
}

void init_all_signals() {
    struct sigaction sa_chld, sa_int, sa_tstp, sa_ttou;

    sa_chld.sa_handler = sigchld_handler;
    sigemptyset(&sa_chld.sa_mask);
    sa_chld.sa_flags = SA_RESTART;
    sigaction(SIGCHLD, &sa_chld, NULL);

    sa_int.sa_handler = sigint_handler;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = 0; // Don't restart, we want to interrupt fgets
    sigaction(SIGINT, &sa_int, NULL);

    sa_tstp.sa_handler = SIG_IGN;
    sigemptyset(&sa_tstp.sa_mask);
    sa_tstp.sa_flags = 0;
    sigaction(SIGTSTP, &sa_tstp, NULL);

    sa_ttou.sa_handler = SIG_IGN;
    sigemptyset(&sa_ttou.sa_mask);
    sa_ttou.sa_flags = 0;
    sigaction(SIGTTOU, &sa_ttou, NULL);
}

int was_sigint() {
    if (sigint_received) {
        sigint_received = 0;
        return 1;
    }
    return 0;
}

int add_job(pid_t pgid, ProcessInfo procs[], int proc_count, JobState state, const char *cmd_line) {
    int job_num = next_job_number++;
    jobs[job_count].job_number = job_num;
    jobs[job_count].pgid = pgid;
    jobs[job_count].proc_count = proc_count;
    for (int i = 0; i < proc_count; i++) {
        jobs[job_count].procs[i] = procs[i];
    }
    jobs[job_count].state = state;
    jobs[job_count].active = 1;
    strncpy(jobs[job_count].command_line, cmd_line, sizeof(jobs[job_count].command_line) - 1);
    jobs[job_count].command_line[sizeof(jobs[job_count].command_line) - 1] = '\0';
    
    job_count++;
    return job_num;
}

Job* find_job(int job_number) {
    for (int i = 0; i < job_count; i++) {
        if (jobs[i].active && jobs[i].job_number == job_number) {
            return &jobs[i];
        }
    }
    return NULL;
}

Job* find_job_by_pid(pid_t pid) {
    for (int i = 0; i < job_count; i++) {
        if (!jobs[i].active) continue;
        for (int j = 0; j < jobs[i].proc_count; j++) {
            if (jobs[i].procs[j].pid == pid) {
                return &jobs[i];
            }
        }
    }
    return NULL;
}

void mark_job_state(int job_number, JobState state) {
    Job *j = find_job(job_number);
    if (j) j->state = state;
}

void remove_job(int job_number) {
    Job *j = find_job(job_number);
    if (j) j->active = 0;
}

int is_tracked_pid(pid_t pid) {
    return find_job_by_pid(pid) != NULL;
}

int is_tracked_job(int job_number) {
    return find_job(job_number) != NULL;
}

int has_stopped_jobs() {
    for (int i = 0; i < job_count; i++) {
        if (jobs[i].active && jobs[i].state == JOB_STOPPED) {
            return 1;
        }
    }
    return 0;
}

int has_any_jobs() {
    for (int i = 0; i < job_count; i++) {
        if (jobs[i].active) return 1;
    }
    return 0;
}

void send_sighup_all() {
    for (int i = 0; i < job_count; i++) {
        if (jobs[i].active) {
            killpg(jobs[i].pgid, SIGHUP);
        }
    }
}

void check_bg_jobs() {
    int status;
    pid_t pid;
    
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
        Job *j = find_job_by_pid(pid);
        if (!j) continue;
        
        int proc_idx = -1;
        for (int i = 0; i < j->proc_count; i++) {
            if (j->procs[i].pid == pid) {
                proc_idx = i;
                break;
            }
        }
        
        if (proc_idx == -1) continue;

        if (WIFEXITED(status) || WIFSIGNALED(status)) {
            j->procs[proc_idx].exited = 1;
            if (j->state == JOB_RUNNING) {
                if (WIFEXITED(status)) {
                    printf("%s with pid %d exited normally\n", j->procs[proc_idx].cmd_name, pid);
                } else if (WIFSIGNALED(status)) {
                    printf("%s with pid %d exited abnormally\n", j->procs[proc_idx].cmd_name, pid);
                }
            }
            
            // Check if all procs in job exited
            int all_exited = 1;
            for (int i = 0; i < j->proc_count; i++) {
                if (!j->procs[i].exited) {
                    all_exited = 0;
                    break;
                }
            }
            if (all_exited) {
                j->active = 0;
            }
        } else if (WIFSTOPPED(status)) {
            // Should be handled by foreground wait, but track just in case
            j->state = JOB_STOPPED;
        } else if (WIFCONTINUED(status)) {
            j->state = JOB_RUNNING;
        }
    }
    sigchld_received = 0;
}

// Compare function for qsort to sort by job_number (which correlates with launch order)
static int compare_jobs(const void *a, const void *b) {
    Job *jobA = (Job *)a;
    Job *jobB = (Job *)b;
    return jobA->job_number - jobB->job_number;
}

void execute_activities() {
    // Collect active jobs
    Job active_jobs[MAX_JOBS];
    int active_count = 0;
    for (int i = 0; i < job_count; i++) {
        if (jobs[i].active) {
            active_jobs[active_count++] = jobs[i];
        }
    }

    // Sort by launch order
    qsort(active_jobs, active_count, sizeof(Job), compare_jobs);

    for (int i = 0; i < active_count; i++) {
        printf("[%d] pgid %d\n", active_jobs[i].job_number, active_jobs[i].pgid);
        for (int j = 0; j < active_jobs[i].proc_count; j++) {
            if (!active_jobs[i].procs[j].exited) {
                printf("  %d %s %s\n", active_jobs[i].procs[j].pid, active_jobs[i].procs[j].cmd_name, 
                       (active_jobs[i].state == JOB_RUNNING) ? "Running" : "Stopped");
            }
        }
    }
}
