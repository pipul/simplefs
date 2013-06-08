TARGET := simplefs
obj-m := $(TARGET).o
simplefs-y := super.o dir.o file.o inode.o bitmap.o

KERNELDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
ins:
	insmod $(TARGET).ko
rm:
	rmmod $(TARGET)
clean:
	rm -rf *.o *.mod.c *.ko
fs:
	go run mkfs.go /dev/mmcblk0p1
dd:
	dd if=/dev/zero of=/dev/mmcblk0p1
test:
	$(MAKE) ins && mount -t simplefs /dev/mmcblk0p1 /tmp/fs
untest:
	umount /tmp/fs && $(MAKE) rm && go run mkfs.go /dev/mmcblk0p1
