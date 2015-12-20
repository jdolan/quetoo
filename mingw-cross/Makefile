# Makefile for Quetoo Windows distributable.

HOST = i686-pc-mingw32
ARCH = i686
DATA = ../../quetoo-data
TARGET = target
INSTALL = $(TARGET)/Quetoo
BINDIR = $(INSTALL)/bin
LIBDIR = $(INSTALL)/lib
DATADIR = $(INSTALL)/share
REMOTE_USER = $(shell whoami)
RSYNC_REPOSITORY = quetoo.org:/opt/rsync/quetoo-mingw/$(ARCH)
RSYNC_TARGET = $(REMOTE_USER)@$(RSYNC_REPOSITORY)
DIST = $(TARGET)/Quetoo-BETA-$(ARCH).zip
HTTP_REPOSITORY = quetoo.org:/var/www/quetoo.org/files
HTTP_TARGET = $(REMOTE_USER)@$(HTTP_REPOSITORY)

all: install-lib

pre-install:
	install -d $(INSTALL)
	cp -r Quetoo-$(ARCH)/* $(INSTALL)
	find $(INSTALL) -name .turd -delete

install: pre-install
	make -C .. DESTDIR=$(realpath $(INSTALL)) install

install-lib: install
	./dllbundler.sh -h $(HOST) \
		$(BINDIR)/quetoo.exe \
		$(BINDIR)/quetoo-dedicated.exe \
		$(BINDIR)/quemap.exe

install-data:
	cp -r $(DATA)/target/* $(DATADIR)

release:
	rsync -rzhP --delete $(INSTALL)/*.* $(BINDIR) $(LIBDIR) $(RSYNC_TARGET)

dist:
	rm -f Quetoo; ln -s $(INSTALL)
	zip -9 -r $(DIST) Quetoo

dist-release:
	rsync -rzhP $(DIST) $(HTTP_TARGET)

clean:
	rm -rf $(TARGET)/*

.PHONY: all pre-install install install-lib install-data release dist dist-release clean

