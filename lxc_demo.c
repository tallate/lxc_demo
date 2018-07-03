/////////////////// 容器demo ///////////////////
// http://www.open-open.com/lib/view/open1427350543512.html //
//  增加了注释                                 //
///////////////////////////////////////////////
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
// 镜像位置
const char *rootfs = "/home/hgc/tools/virtualmachines/ubuntu-16.04/";
//container 主机名
const char *hostname = "mydocker";
static char child_stack[STACK_SIZE];
// 在子进程（容器）内执行的任务
// 因为chroot改变了根目录的位置，所以在保证目标命令存在容器内的基础上（这里是/bin/bash），必须保证该命令依赖的资源同样存在于容器内（动态链接库，使用ldd查看）
char *const child_args[] = {
    "/bin/bash",
    NULL
};
int pipe_fd[2]; //父子进程同步

int child_main(void *args) {
  char c;

  printf("In child process(container)\n");
  chroot(rootfs); //用chroot 切换根目录
  if (errno != 0) {
    perror("chroot()");
    exit(1);
  }
  // clone 调用中的 CLONE_NEWUTS起隔离主机名和域名的作用
  // 这里不能使用sizeof
  sethostname(hostname, strlen(hostname));
  if (errno != 0) {
    perror("sethostname()!");
    exit(1);
  }
  //挂载proc子系统，CLONE_NEWNS 起隔离文件系统作用
  // 需要在rootfs目录下创建proc目录
  mount("proc", "/proc", "proc", 0, NULL);
  if (errno != 0) {
    perror("Mount(proc)");
    exit(1);
  }
  //切换的根目录
  chdir("/");
  close(pipe_fd[1]);
  read(pipe_fd[0], &c, 1);
  //设置veth1 网络
  system("ip link set lo up");
  system("ip link set veth1 up");
  system("ip addr add 169.254.1.2/30 dev veth1");
  //将子进程的镜像替换成bash
  printf("[%s]\n", child_args[0]);
  if (execv(child_args[0], child_args) == -1) {
    perror("execv(path, argv)");
  }
  return 1;
}

struct cgroup *cgroup_control(pid_t pid) {
  struct cgroup *cgroup = NULL;
  int ret;
  ret = cgroup_init();
  char *cgname = malloc(19 * sizeof(char));
  if (ret) {
    printf("error occurs while init cgroup.\n");
    return NULL;
  }
  time_t now_time = time(NULL);
  sprintf(cgname, "mydocker_%d", (int) now_time);
  printf("%s\n", cgname);
  cgroup = cgroup_new_cgroup(cgname);
  if (!cgroup) {
    ret = ECGFAIL;
    printf("Error new cgroup%s\n", cgroup_strerror(ret));
    goto out;
  }
  //添加cgroup memory 和 cpuset子系统
  struct cgroup_controller *cgc = cgroup_add_controller(cgroup, "memory");
  struct cgroup_controller *cgc_cpuset = cgroup_add_controller(cgroup, "cpuset");
  if (!cgc || !cgc_cpuset) {
    ret = ECGINVAL;
    printf("Error add controller %s\n", cgroup_strerror(ret));
    goto out;
  }
  // 内存限制  512M
  if (cgroup_add_value_uint64(cgc, "memory.limit_in_bytes", MEMORY_LIMIT)) {
    printf("Error limit memory.\n");
    goto out;
  }
  //限制只能使用0和1号cpu
  if (cgroup_add_value_string(cgc_cpuset, "cpuset.cpus", "0-1")) {
    printf("Error limit cpuset cpus.\n");
    goto out;
  }
  //限制只能使用0和1块内存
  // TODO 使用0-1作为参数会报错“Invalid argument”，不明白为什么
  if (cgroup_add_value_string(cgc_cpuset, "cpuset.mems", "0")) {
    printf("Error limit cpuset mems.\n");
    goto out;
  }
  ret = cgroup_create_cgroup(cgroup, 0);
  if (ret) {
    printf("Error create cgroup%s\n", cgroup_strerror(ret));
    goto out;
  }
  ret = cgroup_attach_task_pid(cgroup, pid);
  if (ret) {
    printf("Error attach_task_pid %s\n", cgroup_strerror(ret));
    goto out;
  }
  return cgroup;
  out:
  if (cgroup) {
    cgroup_delete_cgroup(cgroup, 0);
    cgroup_free(&cgroup);
  }
  return NULL;
}

int main() {
  char *cmd;
  printf("main process: \n");
  pipe(pipe_fd);
  if (errno != 0) {
    perror("pipe()");
    exit(1);
  }
  // 调用clone创建子进程，传入namespace的几个flag参数，实现namespace的隔离
  // 子进程执行child_main函数，其堆栈空间使用child_stack参数指定
  // clone与线程的实现息息相关：http://www.xuebuyuan.com/1422353.html
  int child_pid = clone(child_main, child_stack + STACK_SIZE, \
            CLONE_NEWNET | CLONE_NEWNS | CLONE_NEWPID | CLONE_NEWIPC | CLONE_NEWUTS | SIGCHLD, NULL);
  struct cgroup *cg = cgroup_control(child_pid);
  // 添加veth pair，设置veth1 在子进程的的namespace，veth0 在父进程的namespace，为了实现container和宿主机之间的网络通信
  // linl3 实现起来太繁琐，借用命令行工具ip 实现
  system("ip link add veth0 type veth peer name veth1");
  asprintf(&cmd, "ip link set veth1 netns %d", child_pid); // asprintf根据字符串长度申请足够的内存空间，但在之后必须手动释放
  system(cmd);
  system("ip link set veth0 up");
  system("ip addr add 169.254.1.1/30 dev veth0");
  free(cmd);
  //等执行以上命令，通知子进程，子进程设置自己的网络
  close(pipe_fd[1]);
  waitpid(child_pid, NULL, 0);
  if (cg) {
    cgroup_delete_cgroup(cg, 0); //删除cgroup 子系统
  }
  printf("child process exited.\n");
  return 0;
}
