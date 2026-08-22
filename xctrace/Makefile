PROG := xctrace
PREFIX ?= /opt/xnuports/opt/xcode-tools

CC := $(shell which clang)
CFLAGS :=
LFLAGS :=

CFLAGS += \
	-Wall \
	-Werror \
	-O2

C_SRCS := \
	xctrace.c \
	xc_record.c \
	xc_export.c

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
	rm -f $(OBJS) $(PROG) *.pkg *.trace *.plist
