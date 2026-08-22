# Top-level Makefile for xcode-tools
# Build system uses bmake and delegates to src/

BUILD_DIR ?= ${.CURDIR}/build
PREFIX ?= /opt/xnuports/opt/xcode-tools
CC := clang

.PHONY: all clean install

all:
	$(MAKE) -C src BUILD_DIR=$(BUILD_DIR) PREFIX=$(PREFIX) CC=$(CC)

clean:
	rm -rf $(BUILD_DIR)
	$(MAKE) -C src clean BUILD_DIR=$(BUILD_DIR)

install: all
	$(MAKE) -C src install BUILD_DIR=$(BUILD_DIR) PREFIX=$(PREFIX) CC=$(CC)
