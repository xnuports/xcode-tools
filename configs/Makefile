TARGET ?= DarwinARM
PREFIX ?= /opt/xnuports/opt/xcode-tools

all:
	@echo "Nothing to do for all"

install: all
	install -d $(DESTDIR)$(PREFIX)/etc
	install -d $(DESTDIR)$(PREFIX)/Developer/SDKs/$(TARGET).sdk
	install -d $(DESTDIR)$(PREFIX)/Developer/Toolchains/$(TARGET).toolchain
	install -m 644 xcrun.ini $(DESTDIR)$(PREFIX)/etc/xcrun.ini
	install -m 644 $(TARGET)SDKSettings.info.ini $(DESTDIR)$(PREFIX)/Developer/SDKs/$(TARGET).sdk/info.ini
	install -m 644 $(TARGET)ToolchainSettings.info.ini $(DESTDIR)$(PREFIX)/Developer/Toolchains/$(TARGET).toolchain/info.ini

clean:
	@echo "Nothing to do for clean"
