
ifneq ($(KERNELRELEASE),)

obj-m := xtion.o
xtion-y := xtion-core.o xtion-control.o xtion-endpoint.o xtion-color.o

else

KDIR ?= /lib/modules/`uname -r`/build

default:
	$(MAKE) -C $(KDIR) M=$$PWD

endif

