# Makefile for Quake2World Windows distributable.

HOST = i686-pc-mingw32
ARCH = i686
DATA = ../../quake2world-data
TARGET = target
INSTALL = $(TARGET)/Quake2World
BINDIR = $(INSTALL)/bin
LIBDIR = $(INSTALL)/lib
DATADIR = $(INSTALL)/share
REMOTE_USER = $(shell whoami)
RSYNC_REPOSITORY = quake2world.net:/opt/rsync/quake2world-mingw/$(ARCH)
RSYNC_TARGET = $(REMOTE_USER)@$(RSYNC_REPOSITORY)
DIST = $(TARGET)/Quake2World-BETA-$(ARCH).zip
HTTP_REPOSITORY = quake2world.net:/var/www/quake2world/files
HTTP_TARGET = $(REMOTE_USER)@$(HTTP_REPOSITORY)

all: install-lib

pre-install:
	install -d $(INSTALL)
	cp -r Quake2World-$(ARCH)/* $(INSTALL)
	find $(INSTALL) -name .turd -delete

install: pre-install
	make -C .. DESTDIR=$(realpath $(INSTALL)) install

install-lib: install
	./dllbundler.sh -h $(HOST) $(BINDIR)/quake2world.exe

install-data:
	cp -r $(DATA)/target/* $(DATADIR)

release:
	rsync -rzhP --delete $(INSTALL)/*.* $(BINDIR) $(LIBDIR) $(RSYNC_TARGET)

dist:
	rm -f Quake2World; ln -s $(INSTALL)
	zip -9 -r $(DIST) Quake2World

dist-release:
	rsync -rzhP $(DIST) $(HTTP_TARGET)

clean:
	rm -rf $(TARGET)/*

.PHONY: all pre-install install install-lib install-data release dist dist-release clean

