PROG := xcode-select
PREFIX ?= /opt/xnuports/opt/xcode-tools

CFLAGS :=
LFLAGS :=

CC := $(shell which clang)

CFLAGS += \
	-Wall \
	-Werror \
	-O2

C_SRCS := \
	xcode-select.c

OBJS := \
	$(patsubst %.c,%.o, $(filter %.c,$(C_SRCS)))

%.c.o:
	$(CC) -x c $(CFLAGS) -c $< -o $@

all: $(OBJS)
	$(CC) $(OBJS) -o $(PROG) $(LFLAGS)

install: all
	install -d $(DESTDIR)$(PREFIX)/bin
	install -s -m 755 $(PROG) $(DESTDIR)$(PREFIX)/bin/$(PROG)

clean:
	rm -f $(OBJS) $(PROG)
