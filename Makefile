# Top-level Makefile for xcode-tools

SUBDIRS := codesign

.PHONY: all clean install

all:
	@for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir; \
	done

clean:
	@for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir clean; \
	done

install: all
	@for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir install; \
	done
