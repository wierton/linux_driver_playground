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

test-async: test-async.c
	gcc $< -o $@.o && ./$@.o $(cdev)

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
