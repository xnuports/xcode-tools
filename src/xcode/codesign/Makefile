# Makefile for codesign

PROG := codesign
OBJDIR := $(BUILD_DIR)/obj/$(PROG)
BINDIR := $(BUILD_DIR)/usr/bin

CC ?= cc
OPENSSL_CFLAGS != pkg-config --cflags openssl 2>/dev/null
LIBCRYPTO_CFLAGS != pkg-config --cflags libcrypto 2>/dev/null
OPENSSL_LDFLAGS != pkg-config --libs openssl 2>/dev/null

CFLAGS := -Wall -Werror -O2 $(OPENSSL_CFLAGS) $(LIBCRYPTO_CFLAGS)

LDFLAGS := $(OPENSSL_LDFLAGS) \
	-framework Security -framework CoreFoundation

SRC := codesign.c cs_blob.c cs_macho.c cs_sign.c cs_verify.c cs_file.c
OBJ := $(SRC:%.c=$(OBJDIR)/%.o)

.PHONY: all clean install

all: $(BINDIR)/$(PROG)

$(BINDIR)/$(PROG): $(OBJ)
	mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(LDFLAGS)

.for src in $(SRC)
$(OBJDIR)/$(src:R).o: $(src)
	mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -c $(src) -o $@
.endfor

clean:
	rm -rf $(OBJDIR)
	rm -f $(BINDIR)/$(PROG)

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/usr/bin
	install -m 755 $(BINDIR)/$(PROG) $(DESTDIR)$(PREFIX)/usr/bin/$(PROG)
