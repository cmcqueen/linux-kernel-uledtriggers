ifneq ($(KERNELRELEASE),)

# kbuild part of makefile
obj-m  := uledtriggers.o

ccflags-y := -I$(src)/include

else

# normal makefile
KDIR ?= /lib/modules/`uname -r`/build

default:
	$(MAKE) -C $(KDIR) M=$$PWD
modules_install:
	$(MAKE) INSTALL_MOD_DIR=kernel/drivers/leds -C $(KDIR) M=$$PWD modules_install
clean:
	$(MAKE) -C $(KDIR) M=$$PWD clean

endif
