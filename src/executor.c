#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include "../include/parser.h"
#include "../include/lexer.h"
#include "../include/hop.h"
#include "../include/reveal.h"
#include "../include/peek.h"
#include "../include/locate.h"
#include "../include/executor.h"

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
    }

    char* exec_path = resolve_executable(cmd->args[0]);
    if (!exec_path) {
        printf("cshell: command not found (%s)\n", cmd->args[0]);
        exit(1);
    }
    cmd->args[cmd->arg_count] = NULL; 
    execv(exec_path, cmd->args);
    exit(1);
}

void execute_pipeline(token tokens[], int count, char *home_dir, char *prev_dir) {
    Command cmds[MAX_CMDS];
    int num_cmds = parse_pipeline(tokens, count, cmds);

    if (num_cmds == 1 && cmds[0].arg_count > 0 && strcmp(cmds[0].args[0], "hop") == 0) {
        token clean_tokens[MAX_ARGS];
        for (int k = 0; k < cmds[0].arg_count; k++) {
            clean_tokens[k].type = WORD;
            strcpy(clean_tokens[k].values, cmds[0].args[k]);
        }
        execute_hop(clean_tokens, cmds[0].arg_count, home_dir, prev_dir);
        return;
    }

    int pipes[MAX_CMDS][2];
    pid_t pids[MAX_CMDS];

    for (int i = 0; i < num_cmds; i++) {
        if (i < num_cmds - 1) pipe(pipes[i]);

        pids[i] = fork();
        if (pids[i] == 0) { 
            
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

        if (i > 0) close(pipes[i - 1][0]);
        if (i < num_cmds - 1) close(pipes[i][1]);
    }

    for (int i = 0; i < num_cmds; i++) {
        waitpid(pids[i], NULL, 0);
    }
}