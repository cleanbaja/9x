KERNEL_SOURCES = kern/init.c      \
		 kern/acpi.c      \
		 kern/acpi_lai.c  \
		 kern/vfs.c       \
		 sys/cpu.c        \
		 sys/tables.c     \
		 sys/irq.c        \
		 sys/apic.c       \
		 sys/hat.c        \
                 sys/timer.c      \
		 lib/builtin.c    \
		 lib/console.c    \
		 lib/strace.c     \
		 lib/log.c        \
		 lib/vec.c        \
		 lib/ubsan.c      \
		 lib/cmdline.c    \
		 vm/vm.c          \
		 vm/virt/virt.c   \
		 vm/virt/invl.c   \
		 vm/phys/phys.c   \
		 vm/phys/zone.c   \
		 vm/alloc.c       \
		 fs/tmpfs.c       \
		 fs/backing.c     \
		 fs/initramfs.c   \
                 proc/smp.c 

KERNEL_ASM = lib/font.asm     \
	     sys/helpers.asm  \
             sys/syscall.asm 

KERNEL_OBJECTS  = $(addprefix $(BUILD_ROOT)/kernel/,$(patsubst %.c, %.o, $(KERNEL_SOURCES))) 
KERNEL_OBJECTS  += $(addprefix $(BUILD_ROOT)/kernel/,$(patsubst %.asm, %.o, $(KERNEL_ASM)))

KCFLAGS = -ffreestanding             \
	  -fno-stack-protector       \
	  -fno-pic                   \
	  -mno-80387                 \
	  -mno-mmx                   \
	  -mno-3dnow                 \
	  -mno-sse                   \
	  -mno-sse2                  \
	  -mno-red-zone              \
	  -fno-omit-frame-pointer    \
          -fplan9-extensions         \
	  -mcmodel=kernel	     \
          -MMD                       \
          -I $(BUILD_ROOT)/gen       \
	  -I include                 \
	  -I third_party/lai/include

$(BUILD_ROOT)/kernel/%.o: src/%.c
	mkdir -p $(@D)
	echo CC $<
	$(CC) $(KCFLAGS) $(CFLAGS) -c -o $@ $<

$(BUILD_ROOT)/kernel/%.o: src/%.asm
	mkdir -p $(@D)
	echo ASM $<
	nasm -f elf64 -o $@ $<

$(BUILD_ROOT)/gen/config.h:
	mkdir -p $(@D)
	cp misc/config.h.in $(BUILD_ROOT)/gen/config.h
	sed -i 's/@VERSION_MAJOR@/$(VMAJOR)/g' $(BUILD_ROOT)/gen/config.h
	sed -i 's/@VERSION_MINOR@/$(VMINOR)/g' $(BUILD_ROOT)/gen/config.h
	sed -i 's/@VERSION_PATCH@/$(VPATCH)/g' $(BUILD_ROOT)/gen/config.h
	sed -i 's/@GIT_COMMIT_HASH@/"$(GIT_VERSION)"/g' $(BUILD_ROOT)/gen/config.h

$(BUILD_ROOT)/9x.elf: $(BUILD_ROOT)/lai/liblai.a $(BUILD_ROOT)/gen/config.h $(KERNEL_OBJECTS)
	mkdir -p $(BUILD_ROOT)/gen
	echo LD $@
	$(LD) $(LDFLAGS) $(KERNEL_OBJECTS) $(BUILD_ROOT)/lai/liblai.a -T misc/kernel.ld -o $@
	./tools/symbols.sh $(BUILD_ROOT)/gen $@
	$(CC) $(KCFLAGS) $(CFLAGS) -c -o $(BUILD_ROOT)/gen/ksym.o $(BUILD_ROOT)/gen/ksym.gen.c
	$(LD) $(LDFLAGS) $(KERNEL_OBJECTS) $(BUILD_ROOT)/lai/liblai.a $(BUILD_ROOT)/gen/ksym.o -T misc/kernel.ld -o $@



# Header dependencies, so that the kernel recompiles when headers changes
HEADER_SRC = $(addprefix $(BUILD_ROOT)/kernel/,$(KERNEL_SOURCES))
HEADER_DEPS = $(HEADER_SRC:.c=.d)
-include $(HEADER_DEPS)


