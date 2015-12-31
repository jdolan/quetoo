# Makefile for Quetoo Linux distributable

ARCH = $(shell uname -m)
DATA = ../../quetoo-data
ARTIFACT = quetoo
TARGET = target
INSTALL = $(TARGET)/$(ARTIFACT)
LIBDIR = $(INSTALL)/lib
BINDIR = $(INSTALL)/bin
DATADIR = $(INSTALL)/share
REMOTE_USER = $(shell whoami)
RSYNC_REPOSITORY = quetoo.org:/opt/rsync/$(ARTIFACT)-linux/$(ARCH)
RSYNC_TARGET = $(REMOTE_USER)@$(RSYNC_REPOSITORY)
DIST = $(TARGET)/$(ARTIFACT)-BETA-$(ARCH).tar.gz
HTTP_REPOSITORY = quetoo.org:/var/www/quetoo.org/files
HTTP_TARGET = $(REMOTE_USER)@$(HTTP_REPOSITORY)

all: install-lib

pre-install:
	install -d $(INSTALL)
	cp -r $(ARTIFACT)/* $(INSTALL)
	find $(INSTALL) -name .turd -delete

configure:
	cd .. ; ./configure \
		LDFLAGS='-Wl,-rpath=XORIGIN/../lib' \
		--build=$(ARCH)-pc-linux \
		--host=$(ARCH)-pc-linux \
		--prefix=/ \
		--with-master
	
compile: configure
	make -C .. all

install: pre-install compile
	make -C .. DESTDIR=$(realpath $(INSTALL)) install

install-lib: install
	./sobundler.sh -d $(LIBDIR) \
		$(BINDIR)/quetoo \
		$(BINDIR)/quetoo-dedicated \
		$(BINDIR)/quemap

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

.PHONY: all pre-install configure compile install install-lib install-data release dist dist-release clean

