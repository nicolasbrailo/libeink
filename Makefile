
SYSROOT=/home/batman/src/xcomp-rpiz-env/mnt/
XCOMPILE=\
	 -target arm-linux-gnueabihf \
	 -mcpu=arm1176jzf-s \
	 --sysroot $(SYSROOT)

#XCOMPILE=

LIBEINK_DEFS=-D epd2in13V4 -D USE_LGPIO_LIB -D RPI -D DEBUG
INCLUDE_DIRS=-I .
# TODO missing flags:
#	-Wpedantic  \
#	-Wstrict-aliasing=2 \
#	-Wextra
#	-Wfloat-equal \
#	-Wredundant-decls \
	-Wmissing-include-dirs \
	-Wpointer-arith \
	-Winit-self \
	-Wimplicit-fallthrough \
	-Wendif-labels \
	-Woverflow \
	-Wformat=2 \
	-Winvalid-pch \
	-ggdb -O0 \
	-std=c99 \
	-fdiagnostics-color=always \
	-D_FILE_OFFSET_BITS=64 \
	-D_POSIX_C_SOURCE=200809 \
	-Wundef \


CFLAGS= \
	$(XCOMPILE) \
	-Wall -Werror \
	-Wno-strict-prototypes \
	-Wno-unused-function \
	-ggdb -O3 -ffunction-sections -fdata-sections -pthread $(LIBEINK_DEFS) $(INCLUDE_DIRS)
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
	# TODO cairo
	true

run: eink
	scp eink StoneBakedMargheritaHomeboard:/home/batman/eink
	ssh StoneBakedMargheritaHomeboard /home/batman/eink/eink pat

