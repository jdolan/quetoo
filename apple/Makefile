# Makefile for Quake2World.app, requires http://macdylibbundler.sourceforge.net/

ARCH = $(shell uname -m)
DATA = ../../quake2world-data
TARGET = target
INSTALL = $(TARGET)/Quake2World
QUAKE2WORLD = $(INSTALL)/Quake2World.app
BINDIR = $(QUAKE2WORLD)/Contents/MacOS
LIBDIR = $(QUAKE2WORLD)/Contents/MacOS/lib
DATADIR = $(QUAKE2WORLD)/Contents/Resources
REMOTE_USER = $(shell whoami)
RSYNC_REPOSITORY = quake2world.net:/opt/rsync/quake2world-apple/$(ARCH)
RSYNC_TARGET = $(REMOTE_USER)@$(RSYNC_REPOSITORY)
DIST = $(TARGET)/Quake2World-BETA.dmg
VOLUME_NAME = "Quake2World BETA"
HTTP_REPOSITORY = quake2world.net:/var/www/quake2world/files
HTTP_TARGET = $(REMOTE_USER)@$(HTTP_REPOSITORY)

all: install-lib

pre-install:
	install -d $(INSTALL)
	cp -r Quake2World/* $(INSTALL)
	find $(INSTALL) -name .turd -delete
	./set-icon.sh icon.icns $(INSTALL)

install: pre-install
	ln -f -s Contents/MacOS $(QUAKE2WORLD)/bin
	ln -f -s Contents/MacOS/lib $(QUAKE2WORLD)/lib
	ln -f -s Contents/Resources $(QUAKE2WORLD)/share
	make -C .. DESTDIR=$(realpath $(QUAKE2WORLD)) install
	rm -f $(QUAKE2WORLD)/bin $(QUAKE2WORLD)/lib $(QUAKE2WORLD)/share

install-lib: install
	dylibbundler -b \
		-x $(BINDIR)/q2wmap \
		-x $(BINDIR)/quake2world \
		-x $(LIBDIR)/default/cgame.so \
		-x $(LIBDIR)/default/game.so \
	-d $(LIBDIR) -of -p @executable_path/lib

install-data:
	cp -r $(DATA)/target/* $(DATADIR)

release:
	rsync -rzh --delete $(BINDIR) $(RSYNC_TARGET)

dist:
	ln -f -s /Applications $(TARGET)/Applications
	hdiutil create $(DIST) -srcfolder $(TARGET) -volname $(VOLUME_NAME)

dist-release:
	rsync -rzh $(DIST) $(HTTP_TARGET)

clean:
	rm -rf $(TARGET)/*

.PHONY: all pre-install install install-lib install-data release dist dist-release clean

