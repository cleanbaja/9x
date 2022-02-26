# Makefile for 9x, a smol unix kernel
# Copyright (c) 2021 cleanbaja, All rights reserved.
# LICENSE: gpl3

# General Configuration ============================================================== 

# Define the build root
BUILD_ROOT ?= build

# Only be verbose if required
ifneq ($(V), 1)
.SILENT:
endif

CFLAGS = -std=gnu11
LDFLAGS = -nostdlib

ifeq ($(OPT), Release)
CFLAGS += -O2 -fsanitize=undefined
else
CFLAGS += -O0 -g 
endif

# Building the kernel/libs ========================================================== 

include src/build.mk
include third_party/lai/build.mk

# Build the kernel by default
all: $(BUILD_ROOT)/src/9x.elf

# ISO Generating/Building ============================================================ 

$(BUILD_ROOT)/limine:
	git clone https://github.com/limine-bootloader/limine.git --branch=latest-binary --depth=1 $@   

$(BUILD_ROOT)/limine/limine-install: $(BUILD_ROOT)/limine
	make -C $(BUILD_ROOT)/limine

$(BUILD_ROOT)/test_image.iso: $(BUILD_ROOT)/src/9x.elf $(BUILD_ROOT)/limine/limine-install
	mkdir -p $(BUILD_ROOT)/isoroot/boot/kernel
	cp $(BUILD_ROOT)/limine/limine-cd.bin $(BUILD_ROOT)/limine/limine-eltorito-efi.bin $(BUILD_ROOT)/isoroot/boot
	cp $(BUILD_ROOT)/limine/limine.sys share/limine.cfg $(BUILD_ROOT)/isoroot/boot
	cp $(BUILD_ROOT)/src/9x.elf $(BUILD_ROOT)/isoroot/boot/kernel
	xorriso -as mkisofs -b boot/limine-cd.bin \
		-no-emul-boot -boot-load-size 4 -boot-info-table \
		--efi-boot boot/limine-eltorito-efi.bin \
		-efi-boot-part --efi-boot-image --protective-msdos-label \
		$(BUILD_ROOT)/isoroot -o $@
	$(BUILD_ROOT)/limine/limine-install $@
	rm -rf $(BUILD_ROOT)/isoroot

iso: $(BUILD_ROOT)/test_image.iso

#  Various util commands ============================================================

.PHONY: run clean
run: $(BUILD_ROOT)/test_image.iso
	printf "\n"
	qemu-system-x86_64 -smp 2 --enable-kvm -cpu max -cdrom $(BUILD_ROOT)/test_image.iso -m 2G -M q35 -debugcon stdio

clean:
	rm -rf $(BUILD_ROOT)

