# Makefile for Quake2World Windows distributable.

MINGW_ARCH = i686
DATA = ../../quake2world-data
TARGET = target
PACKAGE = $(TARGET)/Quake2World
BINDIR = $(PACKAGE)/bin
LIBDIR = $(PACKAGE)/lib
DATADIR = $(PACKAGE)/share
REMOTE_USER = $(shell whoami)
RSYNC_REPOSITORY = quake2world.net:/opt/rsync/quake2world-win32/$(MINGW_ARCH)
RSYNC_TARGET = $(REMOTE_USER)@$(RSYNC_REPOSITORY)
DIST = $(TARGET)/Quake2World-BETA-$(MINGW_ARCH).zip
HTTP_REPOSITORY = quake2world.net:/var/www/quake2world/files
HTTP_TARGET = $(REMOTE_USER)@$(HTTP_REPOSITORY)

all: bundle

pre-install:
	install -d $(PACKAGE)
	cp -r Quake2World-$(MINGW_ARCH)/* $(PACKAGE)
	find $(PACKAGE) -name .turd -delete

install: pre-install
	make -C .. DESTDIR=$(realpath $(PACKAGE)) install

install-data: pre-install
	cp -r $(DATA)/target/* $(DATADIR)

bundle: install
	./dllbundler.sh $(BINDIR)/quake2world.exe

release:
	rsync -avzP --delete $(PACKAGE)/*.* $(BINDIR) $(LIBDIR) $(RSYNC_TARGET)

dist:
	rm -f Quake2World; ln -s $(PACKAGE)
	zip -9 -r $(DIST) Quake2World

dist-release:
	rsync -avzP $(DIST) $(HTTP_TARGET)

clean:
	rm -rf $(TARGET)/*

