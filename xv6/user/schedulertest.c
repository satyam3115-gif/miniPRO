#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
  int n;
  int num_children = 5;

  printf("schedulertest: starting\n");

  int monitor_pid = fork();
  if (monitor_pid == 0) {
    // Monitor process
    for (int i = 0; i < 400; i++) {
      dump_queues();
      pause(1);
    }
    exit(0);
  }

  for (n = 0; n < num_children; n++) {
    int pid = fork();
    if (pid < 0) {
      printf("fork failed\n");
      exit(1);
    }
    if (pid == 0) {
      if (n == 0 || n == 1) {
        // CPU bound process
        // Spins in a loop to consume CPU time
        volatile int dummy = 0;
        for (long long i = 0; i < 1000000000LL; i++) {
          dummy += i;
        }
      } else if (n == 2 || n == 3) {
        // I/O bound process
        // Sleeps frequently to simulate I/O waits and keep priority high
        for (int i = 0; i < 150; i++) {
          pause(1); 
        }
      } else {
        // Mixed process
        // Alternates between CPU bursts and I/O
        volatile int dummy = 0;
        for (int i = 0; i < 15; i++) {
          for (long long j = 0; j < 40000000LL; j++) {
            dummy += j;
          }
          pause(1);
        }
      }
      printf("schedulertest: child %d (type %d) done\n", getpid(), n);
      exit(0);
    }
  }

  // Parent waits for all workers
  for (int i = 0; i < num_children; i++) {
    wait(0);
  }
  
  kill(monitor_pid);
  wait(0); // wait for monitor to exit

  printf("schedulertest: all children completed\n");
  exit(0);
}
