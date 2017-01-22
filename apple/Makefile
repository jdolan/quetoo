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

all: clean install-lib post-install dist

all-data: clean install-lib install-data post-install dist

configure:
	cd .. ; autoreconf -i; ./configure --prefix=/
	
compile: configure
	make -C .. all

pre-install:
	install -d $(INSTALL)
	cp -R Quetoo/* $(INSTALL)
	find $(INSTALL) -name .turd -delete

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
		-x $(BINDIR)/quetoo-dedicated \
		-x $(BINDIR)/quetoo-update \
		-X $(LIBDIR)/default/ai.so \
		-x $(LIBDIR)/default/cgame.so \
		-x $(LIBDIR)/default/game.so \
	-d $(LIBDIR) -of -p @executable_path/lib

install-data:
	cp -R $(DATA)/target/* $(DATADIR)

post-install:
	./set-icon.sh $(DATADIR)/quetoo-folder.icns $(INSTALL)
	./set-icon.sh $(DATADIR)/quemap.icns $(BINDIR)/quemap
	./set-icon.sh $(DATADIR)/quetoo.icns $(BINDIR)/quetoo
	./set-icon.sh $(DATADIR)/quetoo-dedicated.icns $(BINDIR)/quetoo-dedicated
	./set-icon.sh $(DATADIR)/quetoo-update.icns $(BINDIR)/quetoo-update

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

.PHONY: all all-data configure compile pre-install install install-lib install-data post-install release dist dist-release clean

