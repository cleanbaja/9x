override EXT_BUILD_DIR := $(BUILD_DIR)/extensions
override EXT_HDR_DEP   :=

# Import all the driver makefiles...
include $(EXTENSIONS_DIR)/serial/build.mk

# As of right now, we only have one driver (the serial console)
REQUIRED_EXTENSIONS = $(EXT_BUILD_DIR)/serial.kext

.PHONY: extensions_clean
extensions_clean:
	rm -f $(patsubst %.d, %.o, $(EXT_HDR_DEP))
	rm -f $(EXT_HDR_DEP)
	rm -f $(REQUIRED_EXTENSIONS)

