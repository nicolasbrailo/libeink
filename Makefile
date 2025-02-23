SYSROOT=/home/batman/src/xcomp-rpiz-env/mnt/
XCOMPILE=\
	 -target arm-linux-gnueabihf \
	 -mcpu=arm1176jzf-s \
	 --sysroot $(SYSROOT)

# Uncomment to build to local target:
#XCOMPILE=

# TODO liblgpio breaks with these
#	-Wpedantic  \
# -Wextra

CFLAGS= \
	$(XCOMPILE) \
	-fdiagnostics-color=always \
	-ffunction-sections -fdata-sections \
	-ggdb -O3 \
	-std=gnu99 \
	-Wall -Werror \
	-Wendif-labels \
	-Wfloat-equal \
	-Wformat=2 \
	-Wimplicit-fallthrough \
	-Winit-self \
	-Winvalid-pch \
	-Wmissing-include-dirs \
	-Wno-strict-prototypes \
	-Wno-unused-function \
	-Woverflow \
	-Wpointer-arith \
	-Wredundant-decls \
	-Wstrict-aliasing=2 \
	-Wundef \
	-I.

LDFLAGS=-Wl,--gc-sections -lm -lcairo

eink: \
		build/liblgpio/lgCtx.o \
		build/liblgpio/lgDbg.o \
		build/liblgpio/lgGpio.o \
		build/liblgpio/lgHdl.o \
		build/liblgpio/lgPthAlerts.o \
		build/liblgpio/lgPthTx.o \
		build/liblgpio/lgSPI.o \
		build/libeink/eink.o \
		build/main.o
	clang $(CFLAGS) $(LDFLAGS) $^ -o $@


clean:
	rm -rf build

build/%.o: %.c
	mkdir -p $(shell dirname $@)
	clang $(CFLAGS) -c $^ -o $@

cairo: cairo.c
	clang -o cairo cairo.c -lcairo && ./cairo && file ./output.bmp

.PHONY: xcompile-start xcompile-end xcompile-rebuild-sysrootdeps

xcompile-start:
	./rpiz-xcompile/mount_rpy_root.sh ~/src/xcomp-rpiz-env

xcompile-end:
	./rpiz-xcompile/umount_rpy_root.sh ~/src/xcomp-rpiz-env

install_sysroot_deps:
	./rpiz-xcompile/add_sysroot_pkg.sh ~/src/xcomp-rpiz-env http://archive.raspberrypi.com/debian/pool/main/c/cairo/libcairo2-dev_1.16.0-7+rpt1_armhf.deb

.PHONY: deploy run
deploy: eink
	scp eink StoneBakedMargheritaHomeboard:/home/batman/eink
run: deploy
	ssh StoneBakedMargheritaHomeboard /home/batman/eink/eink

