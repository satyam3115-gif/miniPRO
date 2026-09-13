#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>
#include <sys/time.h>
#include "../include/resume.h"
#include "../include/jobs.h"

static volatile sig_atomic_t alarm_fired = 0;

static void alarm_handler(int sig) {
    (void)sig;
    alarm_fired = 1;
}

void execute_resume(char *args[], int arg_count) {
    if (arg_count < 3) {
        printf("resume: invalid syntax\n");
        return;
    }

    if (args[1][0] != '%') {
        printf("resume: invalid syntax\n");
        return;
    }
    int job_num = atoi(&args[1][1]);
    
    int is_fg = 0;
    int is_bg = 0;
    int timeout = 0;

    if (strcmp(args[2], "fg") == 0) {
        is_fg = 1;
    } else if (strcmp(args[2], "bg") == 0) {
        is_bg = 1;
    } else {
        printf("resume: invalid syntax\n");
        return;
    }

    if (arg_count > 3) {
        if (!is_fg || strcmp(args[3], "--timeout") != 0 || arg_count < 5) {
            printf("resume: invalid syntax\n");
            return;
        }
        timeout = atoi(args[4]);
    }

    Job *j = find_job(job_num);
    if (!j) {
        printf("resume: no such job\n");
        return;
    }

    killpg(j->pgid, SIGCONT);
    
    if (is_bg) {
        mark_job_state(job_num, JOB_RUNNING);
        printf("[%d] + Running    %s\n", job_num, j->command_line);
        return;
    }

    if (is_fg) {
        mark_job_state(job_num, JOB_RUNNING);
        printf("%s\n", j->command_line);
        tcsetpgrp(STDIN_FILENO, j->pgid);

        struct sigaction sa, old_sa;
        if (timeout > 0) {
            alarm_fired = 0;
            sa.sa_handler = alarm_handler;
            sigemptyset(&sa.sa_mask);
            sa.sa_flags = 0; // Don't restart waitpid
            sigaction(SIGALRM, &sa, &old_sa);
            alarm(timeout);
        }

        int status;
        pid_t w;
        int job_done = 0;
        int job_stopped = 0;

        // Wait for all processes in the group
        while (1) {
            w = waitpid(-j->pgid, &status, WUNTRACED);
            if (w == -1) {
                if (errno == EINTR) {
                    if (alarm_fired) {
                        break;
                    }
                    if (was_sigint()) {
                        break; // Will let it exit naturally or be handled below
                    }
                    continue;
                }
                if (errno == ECHILD) {
                    job_done = 1;
                }
                break;
            }

            if (WIFSTOPPED(status)) {
                job_stopped = 1;
                break;
            }

            // Check if all are done
            job_done = 1;
            for (int i = 0; i < j->proc_count; i++) {
                if (j->procs[i].pid == w) {
                    j->procs[i].exited = 1;
                }
                if (!j->procs[i].exited) {
                    job_done = 0;
                }
            }
            if (job_done) break;
        }

        if (timeout > 0) {
            alarm(0); // Cancel alarm
            sigaction(SIGALRM, &old_sa, NULL);
        }

        tcsetpgrp(STDIN_FILENO, getpgrp());

        if (alarm_fired) {
            killpg(j->pgid, SIGTERM);
            printf("resume: job timed out\n");
            remove_job(job_num);
        } else if (job_stopped) {
            mark_job_state(job_num, JOB_STOPPED);
            printf("[%d] + Stopped    %s\n", job_num, j->command_line);
        } else if (job_done) {
            remove_job(job_num);
        }
    }
}
