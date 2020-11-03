#include <fcntl.h>
#include <stdio.h>
#include <strings.h>
#include <sys/epoll.h>
#include <sys/select.h>
#include <sys/syscall.h>
#include <unistd.h>

#define FIFO_CLEAR 0x1

int main(int argc, const char *argv[]) {
  fd_set rfds, wfds;

  if (argc < 2) {
    printf("need gblfifo cdev file\n");
    return 0;
  }

  int fd = open(argv[1], O_RDONLY | O_NONBLOCK);
  if (fd < 0) {
    printf("file %s does not exist\n", argv[1]);
  } else {
    struct epoll_event ev_gblfifo;

    if (ioctl(fd, FIFO_CLEAR, 0) < 0) {
      printf("ioctl 'FIFO_CLEAR' failed\n");
      return 0;
    }

    int epfd = epoll_create(1);
    if (epfd < 0) {
      perror("epoll_create");
      return 0;
    }

    bzero(&ev_gblfifo, sizeof(ev_gblfifo));
    ev_gblfifo.events = EPOLLIN | EPOLLPRI;

    int err = epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev_gblfifo);
    if (err < 0) {
      perror("epoll_ctl.add");
      return 0;
    }

    err = epoll_wait(epfd, &ev_gblfifo, 1, 15000);
    if (err < 0) {
      perror("epoll_wait");
      return 0;
    } else if (err == 0) {
      printf("no data input in FIFO within 15 seconds\n");
    } else {
      printf("FIFO is not empty\n");
    }

    err = epoll_ctl(epfd, EPOLL_CTL_DEL, fd, &ev_gblfifo);
    if (err < 0) {
      perror("epoll_ctl.del");
      return 0;
    }
  }

  return 0;
}
