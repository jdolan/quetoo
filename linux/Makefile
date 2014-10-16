# Makefile for Quake2World Linux distributable

ARCH = $(shell uname -m)
DATA = ../../quake2world-data
ARTIFACT = quake2world
TARGET = target
INSTALL = $(TARGET)/$(ARTIFACT)
LIBDIR = $(INSTALL)/lib
BINDIR = $(INSTALL)/bin
DATADIR = $(INSTALL)/share
REMOTE_USER = $(shell whoami)
RSYNC_REPOSITORY = quake2world.net:/opt/rsync/$(ARTIFACT)-linux/$(ARCH)
RSYNC_TARGET = $(REMOTE_USER)@$(RSYNC_REPOSITORY)
DIST = $(TARGET)/$(ARTIFACT)-BETA-$(ARCH).tar.gz
HTTP_REPOSITORY = quake2world.net:/var/www/quake2world.net/files
HTTP_TARGET = $(REMOTE_USER)@$(HTTP_REPOSITORY)

all: install-lib

pre-install:
	install -d $(INSTALL)
	cp -r $(ARTIFACT)/* $(INSTALL)
	find $(INSTALL) -name .turd -delete

install: pre-install
	make -C .. DESTDIR=$(realpath $(INSTALL)) install

install-lib: install
	./sobundler.sh -d $(LIBDIR) $(BINDIR)/quake2world

install-data:
	cp -r $(DATA)/target/* $(DATADIR)

release:
	rsync -rzhP --delete $(INSTALL)/*.* $(BINDIR) $(LIBDIR) $(RSYNC_TARGET)

dist:
	tar -czvf $(DIST) -C $(TARGET) $(ARTIFACT)

dist-release:
	rsync -rzhP $(DIST) $(HTTP_TARGET)

clean:
	rm -rf $(TARGET)/*

.PHONY: all pre-install install install-lib install-data release dist dist-release clean

