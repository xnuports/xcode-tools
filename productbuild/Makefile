PROG := productbuild
PREFIX ?= /opt/xnuports/opt/xcode-tools

CC := $(shell which clang)
CFLAGS := $(shell pkg-config --cflags openssl 2>/dev/null)
LFLAGS := $(shell pkg-config --libs openssl 2>/dev/null) -lz -lcurl -framework Security

CFLAGS += \
	-Wall \
	-Werror \
	-O2

C_SRCS := \
	productbuild.c \
	xar.c \
	payload.c \
	bom.c \
	analyze.c \
	dist.c

OBJS := \
	$(patsubst %.c,%.o, $(filter %.c,$(C_SRCS)))

%.c.o:
	$(CC) -x c $(CFLAGS) -c $< -o $@

all: $(OBJS)
	$(CC) $(OBJS) -o $(PROG) $(LFLAGS)

install: all
	install -d $(DESTDIR)$(PREFIX)/usr/bin
	install -s -m 755 $(PROG) $(DESTDIR)$(PREFIX)/usr/bin/$(PROG)

clean:
	rm -f $(OBJS) $(PROG) *.pkg