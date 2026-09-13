#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include "../include/snoop.h"
#include "../include/executor.h"

// Mac OS doesn't have orig_rax or PTRACE_SYSCALL natively via ptrace in standard ways
// that match Linux. This code assumes it is compiled on a Linux x86_64 environment.
// For robustness on macOS, this file will compile with stubs if Linux headers are missing,
// but since the assignment specifies PTRACE_SYSCALL, we proceed assuming a Linux context.

#if defined(__linux__) && defined(__x86_64__)
#define SNOOP_SUPPORTED 1

#define PTRACE_TRACEME 0
#define PTRACE_ATTACH 16
#define PTRACE_SYSCALL 24
#define PTRACE_GETREGS 12
#define PTRACE_SETOPTIONS 0x4200
#define PTRACE_O_TRACESYSGOOD 1

struct user_regs_struct {
    unsigned long long r15, r14, r13, r12, rbp, rbx, r11, r10;
    unsigned long long r9, r8, rax, rcx, rdx, rsi, rdi, orig_rax;
    unsigned long long rip, cs, eflags, rsp, ss, fs_base, gs_base;
    unsigned long long ds, es, fs, gs;
};

extern long ptrace(int request, pid_t pid, void *addr, void *data);

#else
#define SNOOP_SUPPORTED 0
#endif

// Since compilation might fail on Mac, we will mock the functionality if it's not supported.
#if SNOOP_SUPPORTED

#define MAX_SYSCALLS 500

typedef struct {
    int id;
    const char *name;
    int count;
    double total_time;
    int first_occurrence;
} SyscallStat;

static SyscallStat stats[MAX_SYSCALLS];
static int occurrence_counter = 0;

static const char* get_syscall_name(int id) {
    // Lookup table for common syscalls on x86_64
    switch(id) {
        case 0: return "read";
        case 1: return "write";
        case 2: return "open";
        case 3: return "close";
        case 4: return "stat";
        case 5: return "fstat";
        case 6: return "lstat";
        case 7: return "poll";
        case 8: return "lseek";
        case 9: return "mmap";
        case 10: return "mprotect";
        case 11: return "munmap";
        case 12: return "brk";
        case 13: return "rt_sigaction";
        case 14: return "rt_sigprocmask";
        case 15: return "rt_sigreturn";
        case 16: return "ioctl";
        case 17: return "pread64";
        case 18: return "pwrite64";
        case 19: return "readv";
        case 20: return "writev";
        case 21: return "access";
        case 22: return "pipe";
        case 23: return "select";
        case 24: return "sched_yield";
        case 25: return "mremap";
        case 26: return "msync";
        case 35: return "nanosleep";
        case 39: return "getpid";
        case 41: return "socket";
        case 42: return "connect";
        case 43: return "accept";
        case 44: return "sendto";
        case 45: return "recvfrom";
        case 46: return "sendmsg";
        case 47: return "recvmsg";
        case 48: return "shutdown";
        case 49: return "bind";
        case 50: return "listen";
        case 51: return "getsockname";
        case 52: return "getpeername";
        case 53: return "socketpair";
        case 54: return "setsockopt";
        case 55: return "getsockopt";
        case 56: return "clone";
        case 57: return "fork";
        case 58: return "vfork";
        case 59: return "execve";
        case 60: return "exit";
        case 61: return "wait4";
        case 62: return "kill";
        case 63: return "uname";
        case 72: return "fcntl";
        case 73: return "flock";
        case 74: return "fsync";
        case 75: return "fdatasync";
        case 76: return "truncate";
        case 77: return "ftruncate";
        case 78: return "getdents";
        case 79: return "getcwd";
        case 80: return "chdir";
        case 81: return "fchdir";
        case 82: return "rename";
        case 83: return "mkdir";
        case 84: return "rmdir";
        case 85: return "creat";
        case 86: return "link";
        case 87: return "unlink";
        case 88: return "symlink";
        case 89: return "readlink";
        case 90: return "chmod";
        case 91: return "fchmod";
        case 92: return "chown";
        case 93: return "fchown";
        case 94: return "lchown";
        case 95: return "umask";
        case 96: return "gettimeofday";
        case 97: return "getrlimit";
        case 98: return "getrusage";
        case 99: return "sysinfo";
        case 100: return "times";
        case 101: return "ptrace";
        case 102: return "getuid";
        case 103: return "syslog";
        case 104: return "getgid";
        case 105: return "setuid";
        case 106: return "setgid";
        case 107: return "geteuid";
        case 108: return "getegid";
        case 109: return "setpgid";
        case 110: return "getppid";
        case 111: return "getpgrp";
        case 112: return "setsid";
        case 113: return "setreuid";
        case 114: return "setregid";
        case 115: return "getgroups";
        case 116: return "setgroups";
        case 117: return "setresuid";
        case 118: return "getresuid";
        case 119: return "setresgid";
        case 120: return "getresgid";
        case 121: return "getpgid";
        case 122: return "setfsuid";
        case 123: return "setfsgid";
        case 124: return "getsid";
        case 125: return "capget";
        case 126: return "capset";
        case 127: return "rt_sigpending";
        case 128: return "rt_sigtimedwait";
        case 129: return "rt_sigqueueinfo";
        case 130: return "rt_sigsuspend";
        case 131: return "sigaltstack";
        case 231: return "exit_group";
        case 257: return "openat";
        case 258: return "mkdirat";
        case 259: return "mknodat";
        case 260: return "fchownat";
        case 261: return "futimesat";
        case 262: return "newfstatat";
        case 263: return "unlinkat";
        case 264: return "renameat";
        case 265: return "linkat";
        case 266: return "symlinkat";
        case 267: return "readlinkat";
        case 268: return "fchmodat";
        case 269: return "faccessat";
        case 270: return "pselect6";
        case 271: return "ppoll";
        case 272: return "unshare";
        case 273: return "set_robust_list";
        case 274: return "get_robust_list";
        case 275: return "splice";
        case 276: return "tee";
        case 277: return "sync_file_range";
        case 278: return "vmsplice";
        case 279: return "move_pages";
        case 280: return "utimensat";
        case 281: return "epoll_pwait";
        case 282: return "signalfd";
        case 283: return "timerfd_create";
        case 284: return "eventfd";
        case 285: return "fallocate";
        case 286: return "timerfd_settime";
        case 287: return "timerfd_gettime";
        case 288: return "accept4";
        case 289: return "signalfd4";
        case 290: return "eventfd2";
        case 291: return "epoll_create1";
        case 292: return "dup3";
        case 293: return "pipe2";
        case 294: return "inotify_init1";
        case 295: return "preadv";
        case 296: return "pwritev";
        case 297: return "rt_tgsigqueueinfo";
        case 298: return "perf_event_open";
        case 299: return "recvmmsg";
        case 300: return "fanotify_init";
        case 301: return "fanotify_mark";
        case 302: return "prlimit64";
        case 303: return "name_to_handle_at";
        case 304: return "open_by_handle_at";
        case 305: return "clock_adjtime";
        case 306: return "syncfs";
        case 307: return "sendmmsg";
        case 308: return "setns";
        case 309: return "getcpu";
        case 310: return "process_vm_readv";
        case 311: return "process_vm_writev";
        case 312: return "kcmp";
        case 313: return "finit_module";
        case 314: return "sched_setattr";
        case 315: return "sched_getattr";
        case 316: return "renameat2";
        case 317: return "seccomp";
        case 318: return "getrandom";
        case 319: return "memfd_create";
        case 320: return "kexec_file_load";
        case 321: return "bpf";
        case 322: return "execveat";
        case 323: return "userfaultfd";
        case 324: return "membarrier";
        case 325: return "mlock2";
        case 326: return "copy_file_range";
        case 327: return "preadv2";
        case 328: return "pwritev2";
        case 329: return "pkey_mprotect";
        case 330: return "pkey_alloc";
        case 331: return "pkey_free";
        case 332: return "statx";
    }
    return NULL;
}

static int compare_stats(const void *a, const void *b) {
    SyscallStat *sa = (SyscallStat *)a;
    SyscallStat *sb = (SyscallStat *)b;
    if (sa->count != sb->count) {
        return sb->count - sa->count;
    }
    return sa->first_occurrence - sb->first_occurrence;
}

static double get_time_sec() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

void execute_snoop(char *args[], int arg_count) {
    if (arg_count < 2) {
        printf("snoop: invalid syntax\n");
        return;
    }

    pid_t pid = -1;
    int is_attach = 0;

    if (strcmp(args[1], "-p") == 0) {
        if (arg_count < 3) {
            printf("snoop: invalid syntax\n");
            return;
        }
        pid = atoi(args[2]);
        is_attach = 1;
        
        // Verify process exists
        char path[256];
        snprintf(path, sizeof(path), "/proc/%d", pid);
        if (access(path, F_OK) != 0) {
            printf("snoop: no such process\n");
            return;
        }
    }

    memset(stats, 0, sizeof(stats));
    for (int i = 0; i < MAX_SYSCALLS; i++) {
        stats[i].id = i;
        stats[i].name = get_syscall_name(i);
        stats[i].first_occurrence = -1;
    }

    if (is_attach) {
        if (ptrace(PTRACE_ATTACH, pid, NULL, NULL) == -1) {
            perror("ptrace attach");
            return;
        }
    } else {
        pid = fork();
        if (pid == 0) {
            ptrace(PTRACE_TRACEME, 0, NULL, NULL);
            // Search for executable like executor does
            char *exec_args[MAX_ARGS];
            for (int i = 1; i < arg_count; i++) exec_args[i - 1] = args[i];
            exec_args[arg_count - 1] = NULL;
            
            // For simplicity, just use execvp in child
            execvp(exec_args[0], exec_args);
            printf("snoop: command not found\n");
            exit(127);
        }
    }

    int status;
    waitpid(pid, &status, 0);

    if (WIFEXITED(status) && WEXITSTATUS(status) == 127) {
        // Child failed to exec
        return;
    }

    // Set ptrace options
    ptrace(PTRACE_SETOPTIONS, pid, 0, PTRACE_O_TRACESYSGOOD);

    int in_syscall = 0;
    long current_syscall = -1;
    double entry_time = 0;

    while (1) {
        ptrace(PTRACE_SYSCALL, pid, NULL, NULL);
        waitpid(pid, &status, 0);
        
        if (WIFEXITED(status) || WIFSIGNALED(status)) {
            break;
        }

        if (WIFSTOPPED(status) && WSTOPSIG(status) == (SIGTRAP | 0x80)) {
            struct user_regs_struct regs;
            ptrace(PTRACE_GETREGS, pid, NULL, &regs);
            
            if (!in_syscall) {
                current_syscall = regs.orig_rax;
                entry_time = get_time_sec();
                in_syscall = 1;
            } else {
                double exit_time = get_time_sec();
                if (current_syscall >= 0 && current_syscall < MAX_SYSCALLS) {
                    stats[current_syscall].count++;
                    stats[current_syscall].total_time += (exit_time - entry_time);
                    if (stats[current_syscall].first_occurrence == -1) {
                        stats[current_syscall].first_occurrence = occurrence_counter++;
                    }
                }
                in_syscall = 0;
            }
        }
    }

    // Sort and print
    SyscallStat sorted_stats[MAX_SYSCALLS];
    int sorted_count = 0;
    for (int i = 0; i < MAX_SYSCALLS; i++) {
        if (stats[i].count > 0) {
            sorted_stats[sorted_count++] = stats[i];
        }
    }

    qsort(sorted_stats, sorted_count, sizeof(SyscallStat), compare_stats);

    printf("%-20s %-10s %-10s\n", "syscall", "calls", "time");
    for (int i = 0; i < sorted_count; i++) {
        if (sorted_stats[i].name) {
            printf("%-20s %-10d %.4fs\n", sorted_stats[i].name, sorted_stats[i].count, sorted_stats[i].total_time);
        } else {
            char name[32];
            snprintf(name, sizeof(name), "syscall_%d", sorted_stats[i].id);
            printf("%-20s %-10d %.4fs\n", name, sorted_stats[i].count, sorted_stats[i].total_time);
        }
    }
}

#else

// Mock implementation for non-Linux x86_64 systems (like macOS)
// It just prints an error because we can't use /proc or PTRACE_SYSCALL natively.
void execute_snoop(char *args[], int arg_count) {
    (void)args;
    (void)arg_count;
    printf("snoop: unsupported on this platform (requires Linux x86_64)\n");
}

#endif
