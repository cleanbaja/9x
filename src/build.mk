KERNEL_SOURCES = kern/init.c   \
								 kern/vm.c     \
								 sys/gdt.c     \
								 sys/idt.c     \
								 lib/builtin.c \
								 lib/log.c

KERNEL_ASM = lib/font.asm \
						 sys/helpers.asm

KERNEL_OBJECTS  = $(addprefix $(BUILD_ROOT)/,$(patsubst %.c, %.o, $(KERNEL_SOURCES))) 
KERNEL_OBJECTS  += $(addprefix $(BUILD_ROOT)/,$(patsubst %.asm, %.o, $(KERNEL_ASM)))

KCFLAGS = -ffreestanding       \
					-fno-stack-protector \
					-fno-pic             \
					-mno-80387           \
					-mno-mmx             \
					-mno-3dnow           \
					-mno-sse             \
					-mno-sse2            \
					-mno-red-zone        \
					-mcmodel=kernel	     \
					-I include

$(BUILD_ROOT)/%.o: src/%.c
	mkdir -p $(@D)
	echo CC $<
	$(CC) $(KCFLAGS) $(CFLAGS) -c -o $@ $<

$(BUILD_ROOT)/%.o: src/%.asm
	mkdir -p $(@D)
	echo ASM $<
	nasm -f elf64 -o $@ $<

$(BUILD_ROOT)/src/9x.elf: $(KERNEL_OBJECTS)
	mkdir -p $(@D)
	echo LD $@
	$(LD) $(LDFLAGS) $(KERNEL_OBJECTS) -T share/kernel.ld -o $@

