CONFIG_MODULE_SIG=n
PWD := $(shell pwd) 
KVERSION := $(shell uname -r)
KERNEL_DIR :=/lib/modules/$(shell uname -r)/build

MODULE_NAME = hello
obj-m := $(MODULE_NAME).o

all:
	make -C $(KERNEL_DIR) M=$(PWD) modules
clean:
	rm -f *.ko *.o *.mod.o *.mod *.symvers *.cmd  *.mod.c *.order