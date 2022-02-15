LAI_SOURCES = core/error.c        \
              core/eval.c         \
              core/exec.c         \
              core/exec-operand.c \
              core/libc.c         \
              core/ns.c           \
              core/object.c       \
              core/opregion.c     \
              core/os_methods.c   \
              core/variable.c     \
              core/vsnprintf.c    \
              helpers/pc-bios.c   \
              helpers/pci.c       \
              helpers/resource.c  \
              helpers/sci.c       \
              helpers/pm.c        \
              drivers/ec.c        \
              drivers/timer.c

LAI_OBJECTS  = $(addprefix $(BUILD_ROOT)/,$(patsubst %.c, %.o, $(LAI_SOURCES)))
$(BUILD_ROOT)/%.o: third_party/lai/%.c
	mkdir -p $(@D)
	echo CC $<
	$(CC) $(KCFLAGS) $(CFLAGS) -c -o $@ $<

$(BUILD_ROOT)/lai/liblai.a: $(LAI_OBJECTS)
	mkdir -p $(@D)
	echo LIB $@
	$(AR) rcs $@ $(LAI_OBJECTS)


