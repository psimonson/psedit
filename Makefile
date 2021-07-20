# Simple makefile for gcc written by stext editor.
CC=gcc
CFLAGS=-std=c11 -Wall -O #-g
LDFLAGS=-lncurses

BACKUPS=$(shell find . -iname "*.bak")
SRCDIR=$(shell basename $(shell pwd))
DESTDIR?=
PREFIX?=usr/local
VERSION=1.0

INCDIR=./include
SOURCE=$(wildcard ./src/*.c)
OBJECTS=$(SOURCE:%.c=%.c.o)
DEPS=$(SOURCE:%.c=%.c.d)
TARGET=$(SRCDIR)

.PHONY: all install install-doc install-all uninstall uninstall-doc uninstall-all clean distclean dist
all: $(TARGET)

%.c.d: %.c #$(INCDIR)/*.h
	@set -e; rm -f $@; \
	$(CC) $(CFLAGS) -MM $< > $@; \
	echo Dependency $@ created...

%.c.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c -o $@ $<

$(TARGET): $(DEPS) $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $(OBJECTS) $(LDFLAGS)

install: all
	mkdir -p $(DESTDIR)/$(PREFIX)/bin
	install $(TARGET) $(DESTDIR)/$(PREFIX)/bin

install-doc:
	install -Dm 0644 doc/psedit.1 $(DESTDIR)/$(PREFIX)/share/man/man1
	gzip $(DESTDIR)/$(PREFIX)/share/man/man1/psedit.1

install-all: install install-doc

uninstall:
	rm -f $(DESTDIR)/$(PREFIX)/bin/$(TARGET)

uninstall-doc:
	rm -f $(DESTDIR)/$(PREFIX)/share/man/man1/psedit.1.gz

uninstall-all: uninstall uninstall-doc

clean:
	rm -f $(OBJECTS) $(TARGET)

distclean: clean
ifneq ($(BACKUPS),)
	rm -f $(BACKUPS)
endif
	rm -f $(DEPS)

dist: distclean
	cd .. && tar -cv --exclude=.git ./$(SRCDIR) | xz -9 > $(SRCDIR)-$(VERSION).tar.xz
