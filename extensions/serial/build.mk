SERIAL_DIR     = $(EXTENSIONS_DIR)/serial
SERIAL_SOURCES = $(SERIAL_DIR)/main.c \
		 $(SERIAL_DIR)/rs232.c
SERIAL_OBJECTS = $(SERIAL_SOURCES:.c=.o)

extensions/serial.kext: $(SERIAL_OBJECTS)
	$(COOL_PROMPT) "KEXT" $@
	$(LD) $(LDREALFLAGS) -shared -o $@ $(SERIAL_OBJECTS)

