# Makefile for notarytool

PROG := notarytool
OBJDIR := $(BUILD_DIR)/obj/$(PROG)
BINDIR := $(BUILD_DIR)/usr/bin

CC ?= cc
OPENSSL_CFLAGS != pkg-config --cflags openssl 2>/dev/null
OPENSSL_LDFLAGS != pkg-config --libs openssl 2>/dev/null

CFLAGS := -Wall -Werror -O2 $(OPENSSL_CFLAGS)

LDFLAGS := $(OPENSSL_LDFLAGS) \
	-lcurl -framework Security -framework CoreFoundation

C_SRCS := notarytool.c api.c jwt.c json.c keychain.c
OBJS := $(C_SRCS:%.c=$(OBJDIR)/%.o)

.PHONY: all clean install

all: $(BINDIR)/$(PROG)

$(BINDIR)/$(PROG): $(OBJS)
	mkdir -p $(BINDIR)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

.for src in $(C_SRCS)
$(OBJDIR)/$(src:R).o: $(src)
	mkdir -p $(OBJDIR)
	$(CC) -x c $(CFLAGS) -c $(src) -o $@
.endfor

clean:
	rm -rf $(OBJDIR)
	rm -f $(BINDIR)/$(PROG)

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/usr/bin
	install -m 755 $(BINDIR)/$(PROG) $(DESTDIR)$(PREFIX)/usr/bin/$(PROG)
