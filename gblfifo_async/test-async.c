#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <strings.h>
#include <sys/epoll.h>
#include <sys/select.h>
#include <sys/syscall.h>
#include <unistd.h>

#define FIFO_CLEAR 0x1

static void sigio_handler(int signum) {
  printf("receive a signal from gblfifo\n");
}

int main(int argc, const char *argv[]) {
  fd_set rfds, wfds;

  if (argc < 2) {
    printf("need gblfifo cdev file\n");
    return 0;
  }

  int fd = open(argv[1], O_RDONLY | O_NONBLOCK);
  if (fd < 0) {
    perror("open");
  } else {
    signal(SIGIO, sigio_handler);

    /* set ownership of fd */
    fcntl(fd, F_SETOWN, getpid());

    /* add flag FASYNC */
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | FASYNC);
    while (1) { sleep(1); }
  }

  return 0;
}
