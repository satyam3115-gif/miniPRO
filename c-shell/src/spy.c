#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <ctype.h>
#include "../include/spy.h"

#define MAX_PATH 1024
#define MAX_MEM_FILES 1024

static const char* get_file_type(mode_t mode) {
    if (S_ISREG(mode)) return "REG";
    if (S_ISDIR(mode)) return "DIR";
    if (S_ISCHR(mode)) return "CHR";
    if (S_ISBLK(mode)) return "BLK";
    if (S_ISFIFO(mode)) return "FIFO";
    if (S_ISLNK(mode)) return "LNK";
    if (S_ISSOCK(mode)) return "SOCK";
    return "UNKNOWN";
}

static void print_entry(pid_t pid, const char *fd, const char *type, const char *path) {
    printf("%-5d %-5s %-5s %s\n", pid, fd, type, path);
}

static void read_and_print_symlink(pid_t pid, const char *fd_name, const char *proc_path) {
    char target[MAX_PATH];
    struct stat st;
    
    ssize_t len = readlink(proc_path, target, sizeof(target) - 1);
    if (len != -1) {
        target[len] = '\0';
        if (stat(target, &st) == 0) {
            print_entry(pid, fd_name, get_file_type(st.st_mode), target);
        }
    }
}

static void list_memory_mapped_files(pid_t pid) {
    char maps_path[MAX_PATH];
    snprintf(maps_path, sizeof(maps_path), "/proc/%d/maps", pid);
    
    FILE *f = fopen(maps_path, "r");
    if (!f) return;

    char line[1024];
    char *printed_paths[MAX_MEM_FILES];
    int printed_count = 0;

    while (fgets(line, sizeof(line), f)) {
        char *path_start = strchr(line, '/');
        if (path_start) {
            
            int idx = strcspn(path_start, "\n");
            path_start[idx] = '\0';
            
            
            int is_new = 1;
            for (int i = 0; i < printed_count; i++) {
                if (strcmp(printed_paths[i], path_start) == 0) {
                    is_new = 0;
                    break;
                }
            }
            
            if (is_new && printed_count < MAX_MEM_FILES) {
                struct stat st;
                if (stat(path_start, &st) == 0) {
                    print_entry(pid, "mem", get_file_type(st.st_mode), path_start);
                    printed_paths[printed_count++] = strdup(path_start);
                }
            }
        }
    }
    fclose(f);

    for (int i = 0; i < printed_count; i++) {
        free(printed_paths[i]);
    }
}

static void list_file_descriptors(pid_t pid) {
    char fd_dir_path[MAX_PATH];
    snprintf(fd_dir_path, sizeof(fd_dir_path), "/proc/%d/fd", pid);
    
    DIR *dir = opendir(fd_dir_path);
    if (!dir) return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        
        int is_num = 1;
        for (int i = 0; entry->d_name[i]; i++) {
            if (!isdigit(entry->d_name[i])) {
                is_num = 0;
                break;
            }
        }
        
        if (is_num) {
            char proc_path[MAX_PATH];
            snprintf(proc_path, sizeof(proc_path), "%s/%s", fd_dir_path, entry->d_name);
            read_and_print_symlink(pid, entry->d_name, proc_path);
        }
    }
    closedir(dir);
}

void execute_spy(char *args[], int arg_count) {
    if (arg_count > 2) {
        printf("spy: invalid syntax\n");
        return;
    }

    pid_t target_pid;
    if (arg_count == 1) {
        target_pid = getpid();
    } else {
        target_pid = atoi(args[1]);
        if (target_pid <= 0) {
            printf("spy: no such process\n");
            return;
        }
    }

    char proc_path[MAX_PATH];
    snprintf(proc_path, sizeof(proc_path), "/proc/%d", target_pid);
    struct stat st;
    if (stat(proc_path, &st) != 0) {
        printf("spy: no such process\n");
        return;
    }

    printf("%-5s %-5s %-5s %s\n", "PID", "FD", "TYPE", "PATH");

    snprintf(proc_path, sizeof(proc_path), "/proc/%d/cwd", target_pid);
    read_and_print_symlink(target_pid, "cwd", proc_path);

    snprintf(proc_path, sizeof(proc_path), "/proc/%d/exe", target_pid);
    read_and_print_symlink(target_pid, "txt", proc_path);

    list_memory_mapped_files(target_pid);
    list_file_descriptors(target_pid);
}
