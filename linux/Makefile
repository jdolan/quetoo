# Makefile for Quetoo Linux distributable

ARCH = x86_64
HOST = $(ARCH)-pc-linux
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

all: clean install-lib dist

all-data: clean install-lib install-data dist

configure:
	cd .. ; autoreconf -i; ./configure \
		LDFLAGS='-Wl,-rpath=XORIGIN/../lib' \
		--host=$(HOST) \
		--prefix=/ \
		--with-master
	
compile: configure
	make -C .. all

pre-install:
	install -d $(INSTALL)
	cp -r $(ARTIFACT)/* $(INSTALL)
	find $(INSTALL) -name .turd -delete

install: compile pre-install
	make -C .. DESTDIR=$(realpath $(INSTALL)) install

install-lib: install
	./sobundler.sh -d $(LIBDIR) \
		$(BINDIR)/quetoo \
		$(BINDIR)/quetoo-dedicated \
		$(BINDIR)/quetoo-master \
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

.PHONY: all all-data configure compile pre-install install install-lib install-data release dist dist-release clean

