obj-m  := gblfifo.o
PWD    != pwd
KVER   != uname -r
KDIR   := /lib/modules/$(KVER)/build/
target-ko := $(obj-m:.o=.ko)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

install:
	sudo insmod $(target-ko)

uninstall:
	sudo rmmod $(target-ko)

node:
	sudo mknod gblmem c 230 0
	sudo chown $(shell whoami):$(shell whoami) gblmem

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
