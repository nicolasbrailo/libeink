
SYSROOT=/home/batman/src/xcomp-rpiz-env/mnt/
XCOMPILE=\
	 -target arm-linux-gnueabihf \
	 -mcpu=arm1176jzf-s \
	 --sysroot $(SYSROOT)

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
LDFLAGS=-Wl,--gc-sections -lm

eink: \
		build/liblgpio/lgCtx.o \
		build/liblgpio/lgDbg.o \
		build/liblgpio/lgErr.o \
		build/liblgpio/lgGpio.o \
		build/liblgpio/lgHdl.o \
		build/liblgpio/lgI2C.o \
		build/liblgpio/lgNotify.o \
		build/liblgpio/lgPthAlerts.o \
		build/liblgpio/lgPthTx.o \
		build/liblgpio/lgSerial.o \
		build/liblgpio/lgSPI.o \
		build/liblgpio/lgThread.o \
		build/liblgpio/lgUtil.o \
		build/fonts/font12.o \
		build/fonts/font16.o \
		build/fonts/font20.o \
		build/fonts/font24.o \
		build/fonts/font8.o \
		build/libeink/GUI_BMPfile.o \
		build/libeink/GUI_Paint.o \
		build/libeink/EPD_2in13_V4.o \
		build/libeink/dev_hardware_SPI.o \
		build/libeink/DEV_Config.o \
		build/example/main.o \
		build/example/ImageData2.o \
		build/example/ImageData.o
	clang $(CFLAGS) $(LDFLAGS) $^ -o $@


to_rm_objs:
		build/fonts/font12CN.o \
		build/fonts/font24CN.o \


clean:
	rm -rf build

build/%.o: %.c
	mkdir -p $(shell dirname $@)
	clang $(CFLAGS) -c $^ -o $@


.PHONY: xcompile-start xcompile-end xcompile-rebuild-sysrootdeps

xcompile-start:
	./rpiz-xcompile/mount_rpy_root.sh ~/src/xcomp-rpiz-env

xcompile-end:
	./rpiz-xcompile/umount_rpy_root.sh ~/src/xcomp-rpiz-env

install_sysroot_deps:
	true
