# Makefile for Quake2World.app, requires http://macdylibbundler.sourceforge.net/

ARCH = x86_64
DATA = ../../quake2world-data
TARGET = target
PACKAGE = $(TARGET)/Quake2World
QUAKE2WORLD = $(PACKAGE)/Quake2World.app
BINDIR = $(QUAKE2WORLD)/Contents/MacOS
LIBDIR = $(QUAKE2WORLD)/Contents/lib
DATADIR = $(QUAKE2WORLD)/Contents/Resources
REMOTE_USER = $(shell whoami)
RSYNC_REPOSITORY = quake2world.net:/opt/rsync/quake2world-apple/$(ARCH)
RSYNC_TARGET = $(REMOTE_USER)@$(RSYNC_REPOSITORY)
DIST = $(TARGET)/Quake2World-BETA.dmg
VOLUME_NAME = "Quake2World BETA"
HTTP_REPOSITORY = quake2world.net:/var/www/quake2world/files
HTTP_TARGET = $(REMOTE_USER)@$(HTTP_REPOSITORY)

all: bundle

pre-install:
	install -d $(PACKAGE)
	cp -r Quake2World/* $(PACKAGE)
	find $(PACKAGE) -name .turd -delete
	./set-icon.sh icon.icns $(PACKAGE)

install: pre-install
	ln -f -s Contents/MacOS $(QUAKE2WORLD)/bin
	ln -f -s Contents/MacOS $(QUAKE2WORLD)/lib
	ln -f -s Contents/Resources $(QUAKE2WORLD)/share
	make -C .. DESTDIR=$(realpath $(QUAKE2WORLD)) install
	rm -f $(QUAKE2WORLD)/bin $(QUAKE2WORLD)/lib $(QUAKE2WORLD)/share

install-data: pre-install
	cp -r $(DATA)/target/* $(DATADIR)

bundle: install
	dylibbundler -b \
		-x $(BINDIR)/q2wmap \
		-x $(BINDIR)/quake2world \
		-x $(BINDIR)/default/cgame.so \
		-x $(BINDIR)/default/game.so \
	-d $(LIBDIR) -of -p @executable_path/../lib

release:
	rsync -rzhP --delete $(BINDIR) $(LIBDIR) $(RSYNC_TARGET)

dist:
	ln -f -s /Applications $(TARGET)/Applications
	hdiutil create $(DIST) -srcfolder $(TARGET) -volname $(VOLUME_NAME)

dist-release:
	rsync -rzhP $(DIST) $(HTTP_TARGET)

clean:
	rm -rf $(TARGET)/*
