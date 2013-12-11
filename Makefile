
ifneq ($(KERNELRELEASE),)

obj-m := xtion.o
xtion-y := xtion_driver.o xtion_io.o xtion_endpoint.o

else

KDIR ?= /lib/modules/`uname -r`/build

default:
	$(MAKE) -C $(KDIR) M=$$PWD

endif

