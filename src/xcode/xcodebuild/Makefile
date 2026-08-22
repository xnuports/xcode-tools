# Makefile for xcodebuild

PROG := xcodebuild
OBJDIR := $(BUILD_DIR)/obj/$(PROG)
BINDIR := $(BUILD_DIR)/usr/bin

CC ?= cc
CFLAGS := -Wall -Werror -O2

LFLAGS :=

C_SRCS := ini.c plist.c settings.c project.c xcodebuild.c
OBJS := $(C_SRCS:%.c=$(OBJDIR)/%.o)

.PHONY: all clean install

all: $(BINDIR)/$(PROG)

$(BINDIR)/$(PROG): $(OBJS)
	mkdir -p $(BINDIR)
	$(CC) $(OBJS) -o $@ $(LFLAGS)

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
