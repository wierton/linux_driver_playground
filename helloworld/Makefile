cfiles != find . -name "*.c" 2>/dev/null | sed 's/^\.\///g'
obj-m  := $(cfiles:.c=.o)
PWD    != pwd
KVER   != uname -r
KDIR   := /lib/modules/$(KVER)/build/

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

install:
	sudo insmod $(cfiles:.c=.ko)

uninstall:
	sudo rmsmod $(cfiles:.c=.ko)

clean:
	rm -rf *.o *.mod.c *.mod.o *.ko *.symvers *.order *.a
