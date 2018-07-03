#define _GNU_SOURCE

#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <sched.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/mount.h>
// 必须先安装：sudo apt-get install libcgroup-dev
#include <libcgroup.h>
#include <time.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>

#define STACK_SIZE (1024 * 1024)
#define MEMORY_LIMIT (512*1024*1024)

//const char *rootfs = "/data1/centos6/rootfs/"; //centos6 镜像位置
//const char *rootfs = "/home/hgc/tools/virtualmachines/ubuntu-16.04/"; // 镜像位置
//const char *hostname = "mydocker"; //container 主机名
static char child_stack[STACK_SIZE];
const char *global_value = "abc";

int child_main(void *args) {
  char *const child_args[] = {
      "/bin/bash",
      0
  };
  perror(global_value);
  if (execv(child_args[0], child_args) == -1) {
    perror("execv(path, argv)");
  }
  return 1;
}

int main(int argc, char *argv[]) {
  int child_pid = clone(child_main, child_stack + STACK_SIZE, \
            CLONE_NEWNET | CLONE_NEWNS | CLONE_NEWPID | CLONE_NEWIPC | CLONE_NEWUTS | SIGCHLD, NULL);
  waitpid(child_pid, NULL, 0);
  return 0;
}
