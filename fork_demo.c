#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <sched.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/mount.h>
#include <time.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>

typedef long long LL;

int global_value = 1;

// 1. fork后子进程复制了一份父进程的全局变量，但是它们之间不能互相查看和影响
// 2. fork实现了COW，
int main(int argc, char *argv[]) {
  int pid;
  if ((pid = fork()) == -1) {
    perror("fork()");
    exit(1);
  } else if (pid == 0) {
    printf("%d: %lld\n", pid, &global_value);
    global_value = 2;
    printf("%d: %lld\n", pid, global_value);
    printf("%d: %lld\n", pid, &global_value);
  } else {
    // 父进程
    printf("%d: %lld\n", pid, &global_value);
    global_value = 3;
    sleep(1);
    printf("%d: %lld\n", pid, global_value);
    printf("%d: %lld\n", pid, &global_value);
    waitpid(pid, NULL, 0);
  }
  return 0;
}
