#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>
#include "../include/parser.h"
#include "../include/lexer.h"
#include "../include/hop.h"
#include "../include/reveal.h"
#include "../include/peek.h"
#include "../include/locate.h"
#include "../include/executor.h"
#include "../include/jobs.h"
#include "../include/resume.h"
#include "../include/ping_cmd.h"
#include "../include/spy.h"
#include "../include/snoop.h"

#define MAX_CMDS 128
#define MAX_ARGS 128
#define MAX_FILES 64

typedef struct {
    char *args[MAX_ARGS];
    int arg_count;
    char *in_files[MAX_FILES];
    int in_count;
    char *out_files[MAX_FILES];
    int out_modes[MAX_FILES]; 
    int out_count;
} Command;

static char* resolve_executable(char* name) {
    if (strchr(name, '/')) {
        if (access(name, X_OK) == 0) return strdup(name);
        return NULL;
    }
    
    char *search_name = name;
    int skip_cwd = 0;
    if (name[0] == '%') {
        skip_cwd = 1;
        search_name = name + 1;
    }

    if (!skip_cwd) {
        char cwd[1024];
        getcwd(cwd, sizeof(cwd));
        char full_path[2048];
        snprintf(full_path, sizeof(full_path), "%s/%s", cwd, search_name);
        if (access(full_path, X_OK) == 0) return strdup(full_path);
    }

    char *path_env = getenv("PATH");
    if (path_env) {
        char *path_copy = strdup(path_env);
        char *dir = strtok(path_copy, ":");
        while (dir) {
            char full_path[2048];
            snprintf(full_path, sizeof(full_path), "%s/%s", dir, search_name);
            if (access(full_path, X_OK) == 0) {
                free(path_copy);
                return strdup(full_path);
            }
            dir = strtok(NULL, ":");
        }
        free(path_copy);
    }
    return NULL;
}

static int parse_pipeline(token tokens[], int count, Command cmds[]) {
    int cmd_idx = 0;
    memset(&cmds[cmd_idx], 0, sizeof(Command));

    int actual_count = count;
    for (int i = 0; i < count; i++) {
        if (tokens[i].type == OP_SEMI || tokens[i].type == OP_AMP) {
            actual_count = i;
            break;
        }
    }

    for (int i = 0; i < actual_count; i++) {
        if (tokens[i].type == OP_PIPE) {
            cmd_idx++;
            memset(&cmds[cmd_idx], 0, sizeof(Command));
        } else if (tokens[i].type == OP_LT) {
            i++;
            cmds[cmd_idx].in_files[cmds[cmd_idx].in_count++] = tokens[i].values;
        } else if (tokens[i].type == OP_GT) {
            i++;
            cmds[cmd_idx].out_files[cmds[cmd_idx].out_count] = tokens[i].values;
            cmds[cmd_idx].out_modes[cmds[cmd_idx].out_count++] = 1;
        } else if (tokens[i].type == OP_GTGT) {
            i++;
            cmds[cmd_idx].out_files[cmds[cmd_idx].out_count] = tokens[i].values;
            cmds[cmd_idx].out_modes[cmds[cmd_idx].out_count++] = 2;
        } else if (tokens[i].type == WORD) {
            cmds[cmd_idx].args[cmds[cmd_idx].arg_count++] = tokens[i].values;
        }
    }
    return cmd_idx + 1;
}

static void handle_input_redirection(Command *cmd) {
    if (cmd->in_count == 0) return;

    char tmp_template[] = "/tmp/csh_XXXXXX";
    int tmp_fd = mkstemp(tmp_template);
    unlink(tmp_template); 

    for (int j = 0; j < cmd->in_count; j++) {
        int fd = open(cmd->in_files[j], O_RDONLY);
        if (fd < 0) {
            printf("cshell: no such file or directory\n");
            exit(1);
        }
        char buf[4096];
        ssize_t n;
        while ((n = read(fd, buf, sizeof(buf))) > 0) write(tmp_fd, buf, n);
        close(fd);
    }
    lseek(tmp_fd, 0, SEEK_SET);
    dup2(tmp_fd, STDIN_FILENO);
    close(tmp_fd);
}

static void handle_output_redirection(Command *cmd) {
    if (cmd->out_count == 0) return;

    int out_fds[MAX_FILES];
    for (int j = 0; j < cmd->out_count; j++) {
        int flags = O_WRONLY | O_CREAT | (cmd->out_modes[j] == 2 ? O_APPEND : O_TRUNC);
        out_fds[j] = open(cmd->out_files[j], flags, 0644);
        if (out_fds[j] < 0) {
            printf("cshell: unable to create file for writing\n");
            exit(1);
        }
    }
    
    if (cmd->out_count == 1) {
        dup2(out_fds[0], STDOUT_FILENO);
        close(out_fds[0]);
    } else {
        int tee_pipe[2];
        pipe(tee_pipe);
        pid_t tee_pid = fork();
        if (tee_pid == 0) {
            close(tee_pipe[1]); 
            char buf[4096]; ssize_t n;
            while ((n = read(tee_pipe[0], buf, sizeof(buf))) > 0) {
                for (int j = 0; j < cmd->out_count; j++) write(out_fds[j], buf, n);
            }
            exit(0);
        }
        dup2(tee_pipe[1], STDOUT_FILENO);
        close(tee_pipe[0]);
        close(tee_pipe[1]);
        for (int j = 0; j < cmd->out_count; j++) close(out_fds[j]);
    }
}

static void execute_command(Command *cmd, char *home_dir, char *prev_dir) {
    if (cmd->arg_count == 0) exit(0);

    token clean_tokens[MAX_ARGS];
    for (int k = 0; k < cmd->arg_count; k++) {
        clean_tokens[k].type = WORD;
        strcpy(clean_tokens[k].values, cmd->args[k]);
    }

    if (strcmp(cmd->args[0], "hop") == 0) {
        execute_hop(clean_tokens, cmd->arg_count, home_dir, prev_dir);
        exit(0);
    } else if (strcmp(cmd->args[0], "reveal") == 0) {
        execute_reveal(clean_tokens, cmd->arg_count, home_dir, prev_dir);
        exit(0);
    } else if (strcmp(cmd->args[0], "peek") == 0) {
        execute_peek(clean_tokens, cmd->arg_count);
        exit(0);
    } else if (strcmp(cmd->args[0], "locate") == 0) {
        execute_locate(clean_tokens, cmd->arg_count);
        exit(0);
    } else if (strcmp(cmd->args[0], "activities") == 0 ||
               strcmp(cmd->args[0], "resume") == 0 ||
               strcmp(cmd->args[0], "ping") == 0 ||
               strcmp(cmd->args[0], "spy") == 0 ||
               strcmp(cmd->args[0], "snoop") == 0) {
        // Built-ins shouldn't normally reach here as child processes,
        // but if they do (e.g. part of pipeline), just exit 0 after printing error or running
        printf("cshell: %s not supported in pipeline\n", cmd->args[0]);
        exit(0);
    }

    char* exec_path = resolve_executable(cmd->args[0]);
    if (!exec_path) {
        printf("cshell: command not found (%s)\n", cmd->args[0]);
        exit(127);
    }
    cmd->args[cmd->arg_count] = NULL; 
    execv(exec_path, cmd->args);
    exit(1);
}

static int execute_single_pipeline(token tokens[], int count, char *home_dir, char *prev_dir, int bg) {
    Command cmds[MAX_CMDS];
    int num_cmds = parse_pipeline(tokens, count, cmds);

    if (!bg && num_cmds == 1 && cmds[0].arg_count > 0) {
        char *cmd_name = cmds[0].args[0];
        if (strcmp(cmd_name, "hop") == 0 ||
            strcmp(cmd_name, "activities") == 0 ||
            strcmp(cmd_name, "resume") == 0 ||
            strcmp(cmd_name, "ping") == 0 ||
            strcmp(cmd_name, "spy") == 0 ||
            strcmp(cmd_name, "snoop") == 0) {
            
            token clean_tokens[MAX_ARGS];
            for (int k = 0; k < cmds[0].arg_count; k++) {
                clean_tokens[k].type = WORD;
                strcpy(clean_tokens[k].values, cmds[0].args[k]);
            }
            
            if (strcmp(cmd_name, "hop") == 0) {
                execute_hop(clean_tokens, cmds[0].arg_count, home_dir, prev_dir);
            } else if (strcmp(cmd_name, "activities") == 0) {
                execute_activities();
            } else if (strcmp(cmd_name, "resume") == 0) {
                execute_resume(cmds[0].args, cmds[0].arg_count);
            } else if (strcmp(cmd_name, "ping") == 0) {
                execute_ping(cmds[0].args, cmds[0].arg_count);
            } else if (strcmp(cmd_name, "spy") == 0) {
                execute_spy(cmds[0].args, cmds[0].arg_count);
            } else if (strcmp(cmd_name, "snoop") == 0) {
                execute_snoop(cmds[0].args, cmds[0].arg_count);
            }
            return 0;
        }
    }

    int pipes[MAX_CMDS][2];
    pid_t pids[MAX_CMDS];
    pid_t pgid = 0;
    ProcessInfo procs[MAX_CMDS];

    for (int i = 0; i < num_cmds; i++) {
        if (i < num_cmds - 1) pipe(pipes[i]);

        pids[i] = fork();
        if (pids[i] == 0) {
            if (i == 0) pgid = getpid();
            setpgid(0, pgid);
            
            // Restore signal handlers for children
            struct sigaction sa;
            sa.sa_handler = SIG_DFL;
            sigemptyset(&sa.sa_mask);
            sa.sa_flags = 0;
            sigaction(SIGINT, &sa, NULL);
            sigaction(SIGTSTP, &sa, NULL);
            sigaction(SIGTTOU, &sa, NULL);

            if (bg && i == 0) {
                int devnull = open("/dev/null", O_RDONLY);
                dup2(devnull, STDIN_FILENO);
                close(devnull);
            }

            if (i > 0) dup2(pipes[i - 1][0], STDIN_FILENO);
            if (i < num_cmds - 1) dup2(pipes[i][1], STDOUT_FILENO);

            handle_input_redirection(&cmds[i]);
            handle_output_redirection(&cmds[i]);

            for (int j = 0; j <= i; j++) {
                if (j < num_cmds - 1) {
                    close(pipes[j][0]);
                    close(pipes[j][1]);
                }
            }

            execute_command(&cmds[i], home_dir, prev_dir);
        }

        if (i == 0) pgid = pids[0];
        setpgid(pids[i], pgid);
        
        procs[i].pid = pids[i];
        strncpy(procs[i].cmd_name, cmds[i].args[0], 255);
        procs[i].cmd_name[255] = '\0';
        procs[i].exited = 0;

        if (i > 0) close(pipes[i - 1][0]);
        if (i < num_cmds - 1) close(pipes[i][1]);
    }

    char cmd_line[1024] = "";
    for (int i = 0; i < num_cmds; i++) {
        for (int j = 0; j < cmds[i].arg_count; j++) {
            strcat(cmd_line, cmds[i].args[j]);
            if (j < cmds[i].arg_count - 1) strcat(cmd_line, " ");
        }
        if (i < num_cmds - 1) strcat(cmd_line, " | ");
    }

    if (!bg) {
        tcsetpgrp(STDIN_FILENO, pgid);
        
        int job_num = add_job(pgid, procs, num_cmds, JOB_RUNNING, cmd_line);

        int cmd_not_found = 0;
        int job_stopped = 0;
        int job_done = 0;

        while (1) {
            int status;
            pid_t w = waitpid(-pgid, &status, WUNTRACED);
            if (w == -1) {
                if (errno == EINTR) {
                    if (was_sigint()) {
                        continue;
                    }
                    continue;
                }
                if (errno == ECHILD) {
                    job_done = 1;
                }
                break;
            }
            
            if (WIFEXITED(status) && WEXITSTATUS(status) == 127) {
                cmd_not_found = 1;
            }

            if (WIFSTOPPED(status)) {
                job_stopped = 1;
                break;
            }
            
            Job *j = find_job(job_num);
            if (j) {
                job_done = 1;
                for (int i = 0; i < j->proc_count; i++) {
                    if (j->procs[i].pid == w) {
                        j->procs[i].exited = 1;
                    }
                    if (!j->procs[i].exited) job_done = 0;
                }
            }
            if (job_done) break;
        }

        tcsetpgrp(STDIN_FILENO, getpgrp());

        if (job_stopped) {
            mark_job_state(job_num, JOB_STOPPED);
            printf("[%d] + Stopped    %s\n", job_num, cmd_line);
        } else if (job_done) {
            remove_job(job_num);
        }

        return cmd_not_found ? -1 : 0;
    } else {
        int job_num = add_job(pgid, procs, num_cmds, JOB_RUNNING, cmd_line);
        printf("[%d] %d\n", job_num, pgid);
        fflush(stdout);
        return 0;
    }
}

void execute_pipeline(token tokens[], int count, char *home_dir, char *prev_dir) {
    int start = 0;

    while (start < count) {
        int end = start;
        int bg = 0;

        while (end < count && tokens[end].type != OP_SEMI && tokens[end].type != OP_AMP) {
            end++;
        }

        if (end < count && tokens[end].type == OP_AMP) {
            bg = 1;
        }

        int seg_count = end - start;
        if (seg_count > 0) {
            int result = execute_single_pipeline(tokens + start, seg_count, home_dir, prev_dir, bg);
            if (!bg) {
                check_bg_jobs(); // Still a good place to reap background jobs
                if (result == -1) {
                    break; // stop on command not found in sequential
                }
            }
        }

        if (end < count) end++;
        start = end;
    }
}