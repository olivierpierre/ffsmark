obj-m += flashmon.o
flashmon-objs := flashmon_core.o flashmon_log.o flashmon_finder.o

# Kernel source root directory :
KERN_DIR=/home/pierre/Logiciels/armadeus-5.3/buildroot/output/build/linux-2.6.29.6
# Target architecture :
ARCH=arm
# Development tools prefix :
CROSS_COMPILE=/home/pierre/Logiciels/armadeus-5.3/buildroot/output/host/usr/bin/arm-linux-

all:
	make -C $(KERN_DIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules
	# cleanup
	rm -rf *.o *.mod.c modules.order Module.symvers
	
clean:
	make -C $(KERN_DIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) clean

release:
	@rm -rf /tmp/Flashmon
	@svn export . /tmp/Flashmon
	@mv -f /tmp/Flashmon/Makefile.Armadeus /tmp/Flashmon/Makefile
	@rm /tmp/Flashmon/TODO
	@mv /tmp/Flashmon/UserGuide/FlashmonUserGuide.pdf /tmp/Flashmon
	@rm -r /tmp/Flashmon/UserGuide
	@cd /tmp; tar cvzf Flashmon.tar.gz Flashmon; cd -
	@mv /tmp/Flashmon.tar.gz .
	@rm -rf /tmp/Flashmon
