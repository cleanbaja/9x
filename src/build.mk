KERNEL_SOURCES = kern/init.c    \
                 sys/gdt.c      \
		 sys/idt.c      \
		 sys/cpu.c      \
		 lib/builtin.c  \
		 lib/console.c  \
		 lib/strace.c   \
		 lib/log.c      \
		 vm/vm.c        \
		 vm/virt.c      \
		 vm/phys.c      \
		 vm/alloc.c   

KERNEL_ASM = lib/font.asm     \
	     sys/helpers.asm  \
	     sys/spinlock.asm 

KERNEL_OBJECTS  = $(addprefix $(BUILD_ROOT)/,$(patsubst %.c, %.o, $(KERNEL_SOURCES))) 
KERNEL_OBJECTS  += $(addprefix $(BUILD_ROOT)/,$(patsubst %.asm, %.o, $(KERNEL_ASM)))

KCFLAGS = -ffreestanding           \
	  -fno-stack-protector     \
	  -fno-pic                 \
	  -mno-80387               \
	  -mno-mmx                 \
	  -mno-3dnow               \
	  -mno-sse                 \
	  -mno-sse2                \
	  -mno-red-zone            \
	  -fno-omit-frame-pointer  \
	  -mcmodel=kernel	   \
	  -I include               \
	  -g

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
	./share/symbols.sh $(BUILD_ROOT)/src $@
	$(CC) $(KCFLAGS) $(CFLAGS) -c -o $(BUILD_ROOT)/src/ksym.o $(BUILD_ROOT)/src/ksym.gen.c
	$(LD) $(LDFLAGS) $(KERNEL_OBJECTS) $(BUILD_ROOT)/src/ksym.o -T share/kernel.ld -o $@


