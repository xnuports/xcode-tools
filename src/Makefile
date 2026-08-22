# Makefile for src/

.PHONY: all clean install

all:
	$(MAKE) -C xcode BUILD_DIR=$(BUILD_DIR) PREFIX=$(PREFIX) CC=$(CC)

clean:
	$(MAKE) -C xcode clean BUILD_DIR=$(BUILD_DIR)

install:
	$(MAKE) -C xcode install BUILD_DIR=$(BUILD_DIR) PREFIX=$(PREFIX) CC=$(CC)
