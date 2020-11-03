#include <fcntl.h>
#include <stdio.h>
#include <sys/select.h>
#include <sys/syscall.h>
#include <unistd.h>

#define FIFO_CLEAR 0x1

int main(int argc, const char *argv[]) {
  int fd, num;

  char rd_ch[BUFSIZ];
  fd_set rfds, wfds;

  if (argc < 2) {
    printf("need gblfifo cdev file\n");
    return 0;
  }

  fd = open(argv[1], O_RDONLY | O_NONBLOCK);
  if (fd < 0) {
    printf("file %s does not exist\n", argv[1]);
  } else {
    /* clear */
    if (ioctl(fd, FIFO_CLEAR, 0) < 0) {
      printf("ioctl 'FIFO_CLEAR' failed\n");
      return 0;
    }

    while (1) {
      FD_ZERO(&rfds);
      FD_ZERO(&wfds);

      FD_SET(fd, &rfds);
      FD_SET(fd, &wfds);

      select(fd + 1, &rfds, &wfds, NULL, NULL);

      sleep(1);

      if (FD_ISSET(fd, &rfds)) 
        printf("poll: can be read\n");

      if (FD_ISSET(fd, &wfds)) 
        printf("poll: can be written\n");
    }
  }

  return 0;
}
