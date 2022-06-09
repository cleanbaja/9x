SERIAL_DIR     = $(EXTENSIONS_DIR)/serial
SERIAL_SOURCES = $(SERIAL_DIR)/main.c \
		 $(SERIAL_DIR)/rs232.c
SERIAL_OBJECTS = $(addprefix $(BUILD_DIR), $(patsubst %.c, %.o, $(SERIAL_SOURCES:$(SOURCE_DIR)%=%)))

$(EXT_BIN_DIR)/serial.kext: $(SERIAL_OBJECTS)
	$(COOL_PROMPT) "KEXT" $@
	mkdir -p $(@D)
	$(LD) $(LDREALFLAGS) -shared -o $@ $(SERIAL_OBJECTS)

