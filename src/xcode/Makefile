# Makefile for src/xcode/

SUBDIRS := \
	codesign \
	devicectl \
	notarytool \
	pkgbuild \
	productbuild \
	simctl \
	xcode-select \
	xcodebuild \
	xcrun \
	xctrace

.PHONY: all clean install

all:
	@for dir in $(SUBDIRS); do \
		echo ">>> Building $$dir"; \
		$(MAKE) -C $$dir BUILD_DIR=$(BUILD_DIR) PREFIX=$(PREFIX) CC=$(CC); \
	done

clean:
	@for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir clean BUILD_DIR=$(BUILD_DIR); \
	done

install: all
	@for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir install BUILD_DIR=$(BUILD_DIR) PREFIX=$(PREFIX); \
	done
