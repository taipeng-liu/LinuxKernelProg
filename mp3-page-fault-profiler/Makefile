EXTRA_CFLAGS +=
APP_EXTRA_FLAGS:= -O2 -ansi -pedantic
KERNEL_SRC:= /lib/modules/$(shell uname -r)/build
SUBDIR= $(PWD)
GCC:=gcc
RM:=rm

.PHONY : clean

all: clean modules work monitor
test: clean modules work monitor insm testcase1 testcase2 remm clean2

obj-m:= mp3.o

modules:
	$(MAKE) -C $(KERNEL_SRC) M=$(SUBDIR) modules

work: work.c
	$(GCC) -o work work.c

monitor: monitor.c
	$(GCC) -o monitor monitor.c

clean:
	$(RM) -f work userapp monitor *~ *.ko *.o *.mod.c Module.symvers modules.order

clean2:
	$(RM) -f work userapp monitor *~ *.ko *.o *.mod.c Module.symvers modules.order

insm:
	-sudo rmmod mp3
	sudo dmesg -C
	sudo insmod mp3.ko
	dmesg

testcase1:
	./testcase1.sh

testcase2:
	./testcase2.sh

remm:
	sudo dmesg -C
	sudo rmmod mp3
	dmesg
