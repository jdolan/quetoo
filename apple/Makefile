# Makefile for Quetoo.app, requires http://macdylibbundler.sourceforge.net/

ARCH = x86_64
DATA = ../../quetoo-data
TARGET = target
INSTALL = $(TARGET)/Quetoo
QUETOO = $(INSTALL)/Quetoo.app
BINDIR = $(QUETOO)/Contents/MacOS
LIBDIR = $(QUETOO)/Contents/MacOS/lib
DATADIR = $(QUETOO)/Contents/Resources
REMOTE_USER = $(shell whoami)
RSYNC_REPOSITORY = quetoo.org:/opt/rsync/quetoo-apple/$(ARCH)
RSYNC_TARGET = $(REMOTE_USER)@$(RSYNC_REPOSITORY)
DIST = $(TARGET)/Quetoo-BETA-$(ARCH).dmg
VOLUME_NAME = "Quetoo BETA $(ARCH)"
HTTP_REPOSITORY = quetoo.org:/var/www/quetoo.org/files
HTTP_TARGET = $(REMOTE_USER)@$(HTTP_REPOSITORY)

all: install-lib

configure:
	cd .. ; ./configure --prefix=/
	
compile: configure
	make -C .. all

pre-install:
	install -d $(INSTALL)
	cp -R Quetoo/* $(INSTALL)
	find $(INSTALL) -name .turd -delete
	./set-icon.sh icon.icns $(INSTALL)

install: compile pre-install
	ln -f -s Contents/MacOS $(QUETOO)/bin
	ln -f -s Contents/MacOS/lib $(QUETOO)/lib
	ln -f -s Contents/Resources $(QUETOO)/share
	make -C .. DESTDIR=$(realpath $(QUETOO)) install
	rm -f $(QUETOO)/bin $(QUETOO)/lib $(QUETOO)/share

install-lib: install
	dylibbundler -b \
		-x $(BINDIR)/quemap \
		-x $(BINDIR)/quetoo \
		-x $(LIBDIR)/default/cgame.so \
		-x $(LIBDIR)/default/game.so \
	-d $(LIBDIR) -of -p @executable_path/lib

install-data:
	cp -R $(DATA)/target/* $(DATADIR)

release:
	rsync -rzhP --delete $(BINDIR) $(RSYNC_TARGET)
	rsync -rzhP --delete $(DATADIR)/*.* $(RSYNC_TARGET)/Resources/

dist:
	ln -f -s /Applications $(TARGET)/Applications
	hdiutil create -ov $(DIST) -srcfolder $(TARGET) -volname $(VOLUME_NAME)

dist-release:
	rsync -rzhP $(DIST) $(HTTP_TARGET)

clean:
	rm -rf $(TARGET)/*

.PHONY: all configure compile pre-install install install-lib install-data release dist dist-release clean

