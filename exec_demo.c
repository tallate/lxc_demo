#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>


int main(void) {
  pid_t pid = fork();
  char *argv[] = {"/bin/ls", "/", NULL};

  if (pid == 0) // this is the child process
  {
    execv("/bin/ls", argv);
    // the program should not reach here, or it means error occurs during execute the ls command.
    printf("command ls is not found, error code: %d(%s)", errno, strerror(errno));
  } else {
    waitpid(pid, NULL, 0);
  }
  return 0;
}

