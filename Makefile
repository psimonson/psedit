# Simple makefile for gcc written by stext editor.
CC=gcc
CFLAGS=-std=c11 -W -O0 -g #-I$(INCDIR)
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

.PHONY: all install uninstall clean distclean dist
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
	install $(TARGET) $(DESTDIR)/$(PREFIX)/bin

uninstall:
	rm -f $(DESTDIR)/$(PREFIX)/bin/$(TARGET)

clean:
	rm -f $(OBJECTS) $(TARGET)

distclean: clean
ifneq ($(BACKUPS),)
	rm -f $(BACKUPS)
endif
	rm -f $(DEPS)

dist: distclean
	cd .. && tar -cv --exclude=.git ./$(SRCDIR) | xz -9 > $(SRCDIR)-$(VERSION).tar.xz
