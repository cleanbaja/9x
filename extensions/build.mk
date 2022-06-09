EXT_BIN_DIR=$(BUILD_DIR)/bin
include $(EXTENSIONS_DIR)/serial/build.mk

# As of right now, we only have one driver (the serial console)
REQUIRED_DRIVERS = $(EXT_BIN_DIR)/serial.kext

.PHONY: extensions_clean
extensions_clean:
	rm -f $(REQUIRED_DRIVERS)

