# Makefile for 9x, a smol unix kernel
# Copyright (c) 2021 cleanbaja, All rights reserved.
# LICENSE: gpl3

# General Configuration ============================================================== 

# Define the build/install roots
BUILD_ROOT ?= build
DESTDIR ?= /boot

# Only be verbose if required
ifneq ($(V), 1)
.SILENT:
endif

CFLAGS = -std=gnu11
LDFLAGS = -nostdlib

ifeq ($(OPT), Release)
CFLAGS += -O2
else
CFLAGS += -O0 -g 
endif

.PHONY: all
all: $(BUILD_ROOT)/9x.elf

# Kernel version stuff
VMAJOR := 0
VMINOR := 4
VPATCH := 1
GIT_VERSION := $(shell git rev-parse --short HEAD)

# Building the kernel/libs ========================================================== 

include src/build.mk
include third_party/lai/build.mk

# Building the initrd ================================================================

$(BUILD_ROOT)/initrd.img: $(BUILD_ROOT)/9x.elf
	./tools/mkinitrd.py
	mv initrd.img $(BUILD_ROOT)/initrd.img

# ISO Generating/Building ============================================================ 

$(BUILD_ROOT)/limine:
	git clone https://github.com/limine-bootloader/limine.git --branch=latest-binary --depth=1 $@   

$(BUILD_ROOT)/limine/limine-install: $(BUILD_ROOT)/limine
	make -C $(BUILD_ROOT)/limine

$(BUILD_ROOT)/test_image.iso: $(BUILD_ROOT)/initrd.img $(BUILD_ROOT)/limine/limine-install
	mkdir -p $(BUILD_ROOT)/isoroot/boot/kernel
	cp $(BUILD_ROOT)/limine/limine-cd.bin $(BUILD_ROOT)/limine/limine-eltorito-efi.bin $(BUILD_ROOT)/isoroot/boot
	cp $(BUILD_ROOT)/limine/limine.sys misc/limine.cfg $(BUILD_ROOT)/isoroot/boot
	cp $(BUILD_ROOT)/9x.elf $(BUILD_ROOT)/isoroot/boot/kernel
	cp $(BUILD_ROOT)/initrd.img $(BUILD_ROOT)/isoroot/boot/initrd.img
	xorriso -as mkisofs -b boot/limine-cd.bin \
		-no-emul-boot -boot-load-size 4 -boot-info-table \
		--efi-boot boot/limine-eltorito-efi.bin \
		-efi-boot-part --efi-boot-image --protective-msdos-label \
		$(BUILD_ROOT)/isoroot -o $@
	$(BUILD_ROOT)/limine/limine-install $@
	rm -rf $(BUILD_ROOT)/isoroot

iso: $(BUILD_ROOT)/test_image.iso

#  Various util commands ============================================================

.PHONY: run clean install
run: $(BUILD_ROOT)/test_image.iso
	printf "\n"
	qemu-system-x86_64 -s -monitor telnet:localhost:4321,server,nowait -smp 11 -vnc :0 -cpu max,-la57 -cdrom $(BUILD_ROOT)/test_image.iso -m 2G -M q35 -debugcon stdio

clean:
	rm -rf $(BUILD_ROOT)

install:
	mkdir -p $(DESTDIR)/kernel/
	cp $(BUILD_ROOT)/9x.elf $(DESTDIR)/kernel/9x.elf
	cp misc/limine.cfg $(DESTDIR)/limine.cfg
	cp $(BUILD_ROOT)/initrd.img $(DESTDIR)/initrd.img

