EXTRA_CFLAGS +=
APP_EXTRA_FLAGS:= -O2 -ansi -pedantic
KERNEL_SRC:= /lib/modules/$(shell uname -r)/build
SUBDIR= $(PWD)
GCC:=gcc
RM:=rm

.PHONY : clean

all: clean modules app
test: clean modules app insm testcase remm clean2

obj-m:= mp2.o

modules:
	$(MAKE) -C $(KERNEL_SRC) M=$(SUBDIR) modules

app: userapp.c userapp.h
	$(GCC) -o userapp userapp.c
clean:
	$(RM) -f userapp *~ *.ko *.o *.mod.c Module.symvers modules.order
clean2:
	$(RM) -f userapp *~ *.ko *.o *.mod.c Module.symvers modules.order
insm:
	-sudo rmmod mp2
	sudo dmesg -C
	sudo insmod mp2.ko
	dmesg
testcase:
	./userapp 10000 5
remm:
	sudo dmesg -C
	sudo rmmod mp2
	dmesg
