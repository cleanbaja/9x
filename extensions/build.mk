include $(EXTENSIONS_DIR)/serial/build.mk

# As of right now, we only have one driver (the serial console)
REQUIRED_DRIVERS = $(EXTENSIONS_DIR)/serial.kext

.PHONY: extensions extensions_clean
extensions: $(REQUIRED_DRIVERS)
extensions_clean:
	rm -f $(REQUIRED_DRIVERS)
	find $(EXTENSIONS_DIR) -name '*.o' -delete

