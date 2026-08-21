TARGET ?= DarwinARM
PREFIX ?= /opt/xnuports/opt/xcode-tools

TOOLCHAIN_BIN := $(PREFIX)/Developer/Toolchains/$(TARGET).toolchain/usr/bin

all:
	@echo "Nothing to do for all"

install: all
	install -d $(DESTDIR)$(PREFIX)/bin
	install -d $(DESTDIR)$(PREFIX)/Developer/SDKs/$(TARGET).sdk/usr/bin
	install -d $(DESTDIR)$(TOOLCHAIN_BIN)
	install -m 755 xcrun-tool.sh $(DESTDIR)$(PREFIX)/bin/xcrun-tool
	install -m 755 cc.sh $(DESTDIR)$(TOOLCHAIN_BIN)/cc
	install -m 755 cpp.sh $(DESTDIR)$(TOOLCHAIN_BIN)/cpp
	install -m 755 c++.sh $(DESTDIR)$(TOOLCHAIN_BIN)/c++
	install -m 755 clang.sh $(DESTDIR)$(TOOLCHAIN_BIN)/clang
	ln -sf clang $(DESTDIR)$(TOOLCHAIN_BIN)/clang++

clean:
	@echo "Nothing to do for clean"
