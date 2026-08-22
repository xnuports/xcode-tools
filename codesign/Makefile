# Makefile for codesign

PROG := codesign

PREFIX := /usr/local

CC := /usr/bin/clang
CFLAGS := -Wall -Werror -O2 \
	$(shell pkg-config --cflags openssl 2>/dev/null) \
	$(shell pkg-config --cflags libcrypto 2>/dev/null)

LDFLAGS := $(shell pkg-config --libs openssl 2>/dev/null) \
	-framework Security -framework CoreFoundation

SRC := codesign.c cs_blob.c cs_macho.c cs_sign.c cs_verify.c cs_file.c
OBJ := $(SRC:.c=.o)

all: $(PROG)

$(PROG): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(LDFLAGS)

%.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(PROG)

install: $(PROG)
	install -d $(PREFIX)/bin
	install -m 755 $(PROG) $(PREFIX)/bin/$(PROG)

.PHONY: all clean install
