obj-m  := gblfifo.o
PWD    != pwd
KVER   != uname -r
KDIR   := /lib/modules/$(KVER)/build/
cdev   := gblfifo
target-ko := $(obj-m:.o=.ko)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

install:
	sudo insmod $(target-ko)

uninstall:
	sudo rmmod $(target-ko)

node:
	sudo mknod $(cdev) c 230 0
	sudo chown $(shell whoami):$(shell whoami) $(cdev)

test-poll: test-poll.c
test-epoll: test-epoll.c
test-poll test-epoll:
	gcc $< -o $@.o && ./$@.o $(cdev)

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
