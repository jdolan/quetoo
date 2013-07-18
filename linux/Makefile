# Makefile for Quake2World Linux distributable

ARCH = $(shell uname -m)
DATA = ../../quake2world-data
TARGET = target
PACKAGE = $(TARGET)/quake2world
LIBDIR = $(PACKAGE)/lib
BINDIR = $(PACKAGE)/bin
DATADIR = $(PACKAGE)/share
REMOTE_USER = $(shell whoami)
RSYNC_REPOSITORY = quake2world.net:/opt/rsync/quake2world-linux/$(ARCH)
RSYNC_TARGET = $(REMOTE_USER)@$(RSYNC_REPOSITORY)
DIST = $(TARGET)/quake2world-BETA-$(ARCH).tar.gz
HTTP_REPOSITORY = quake2world.net:/var/www/quake2world/files
HTTP_TARGET = $(REMOTE_USER)@$(HTTP_REPOSITORY)

all: install-lib

pre-install:
	install -d $(PACKAGE)
	cp -r quake2world/* $(PACKAGE)
	find $(PACKAGE) -name .turd -delete

install: pre-install
	make -C .. DESTDIR=$(realpath $(PACKAGE)) install

install-lib: install
	touch $(LIBDIR)

install-data:
	cp -r $(DATA)/target/* $(DATADIR)

release:
	rsync -rzhP --delete $(PACKAGE)/*.* $(BINDIR) $(LIBDIR) $(RSYNC_TARGET)

dist:
	tar -czvf $(DIST) -C $(TARGET) quake2world

dist-release:
	rsync -rzhP $(DIST) $(HTTP_TARGET)

clean:
	rm -rf $(TARGET)/*

.PHONY: all pre-install install install-lib install-data release dist dist-release clean

