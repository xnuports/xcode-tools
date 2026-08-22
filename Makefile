PREFIX ?= /opt/xnuports/opt/xcode-tools
DEVELOPER_DIR ?= $(PREFIX)/Developer

# configs/ and scripts/ carry the deprecated DarwinARM SDK/toolchain shims;
# they are not built or installed. A replacement SDK layout is pending.
DIRS := \
	xcrun \
	xcode-select \
	xcodebuild \
	pkgbuild \
	notarytool \
	productbuild \
	xctrace

define do_make
	@for dir in $1; do \
		make -C $$dir DESTDIR=$(DESTDIR) PREFIX=$(PREFIX) $2; \
	done
endef

all:
	$(call do_make, $(DIRS), all)

install: all
	$(call do_make, $(DIRS), install)
	install -d $(DESTDIR)$(PREFIX)/usr/bin
	install -m 755 scripts/xcrun-tool.sh $(DESTDIR)$(PREFIX)/usr/bin/xcrun-tool
ifndef DESTDIR
	xcode-select --switch $(DEVELOPER_DIR)
endif

clean:
	$(call do_make, $(DIRS), clean)
