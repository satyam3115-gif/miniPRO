#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <ctype.h>
#include "../include/ping_cmd.h"
#include "../include/jobs.h"

static int is_valid_number(const char *str) {
    if (!str || *str == '\0') return 0;
    for (int i = 0; str[i]; i++) {
        if (!isdigit(str[i])) return 0;
    }
    return 1;
}

void execute_ping(char *args[], int arg_count) {
    if (arg_count != 3) {
        printf("ping: invalid syntax\n");
        return;
    }

    if (!is_valid_number(args[2])) {
        printf("ping: invalid syntax\n");
        return;
    }

    int sig_num = atoi(args[2]);
    int actual_sig = sig_num % 64;

    const char *target = args[1];
    
    if (target[0] == '%') {
        int job_num = atoi(&target[1]);
        Job *j = find_job(job_num);
        if (!j) {
            printf("ping: no such process found\n");
            return;
        }
        killpg(j->pgid, actual_sig);
        printf("Sent signal %d to %s\n", sig_num, target);
    } else {
        if (!is_valid_number(target)) {
            // Technically the spec says ping: no such process found if it's just invalid, 
            // but let's assume valid pid. If it's invalid pid format, it won't be tracked anyway.
            printf("ping: no such process found\n");
            return;
        }
        pid_t pid = (pid_t)atoi(target);
        if (!is_tracked_pid(pid)) {
            printf("ping: no such process found\n");
            return;
        }
        kill(pid, actual_sig);
        printf("Sent signal %d to %s\n", sig_num, target);
    }
}
