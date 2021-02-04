# Makefile for Quetoo Linux distributable, requires awscli.

ARCH = x86_64
BUILD = $(ARCH)-pc-linux
TARGET = target
INSTALL = $(TARGET)/quetoo
BINDIR = $(INSTALL)/bin
LIBDIR = $(INSTALL)/lib
DATADIR = $(INSTALL)/share
INSTALLER = quetoo-installer-small.jar
SNAPSHOT = $(TARGET)/quetoo-BETA-$(BUILD).tar.gz
QUETOO_S3_BUCKET = s3://quetoo
QUETOO_DATA_S3_BUCKET = s3://quetoo-data

all: clean install-lib install-installer snapshot

all-data: clean install-lib install-installer install-data snapshot

configure:
	cd .. ; autoreconf -i; ./configure CFLAGS='-g -O2' LDFLAGS='-Wl,-rpath=XORIGIN/../lib' --host=$(BUILD) --prefix=/

compile: configure
	make -C .. all

pre-install:
	install -d $(INSTALL)
	cp -r quetoo/* $(INSTALL)
	find $(INSTALL) -name .turd -delete

install: compile pre-install
	make -C .. DESTDIR=$(realpath $(INSTALL)) install

install-lib: install
	./sobundler.sh -d $(LIBDIR) \
		$(BINDIR)/quemap \
		$(BINDIR)/quetoo \
		$(BINDIR)/quetoo-dedicated \
		$(BINDIR)/quetoo-master \
		$(LIBDIR)/default/ai.so \
		$(LIBDIR)/default/cgame.so \
		$(LIBDIR)/default/game.so

install-installer:
	aws s3 cp $(QUETOO_S3_BUCKET)/snapshots/$(INSTALLER) $(LIBDIR)/

install-data:
	aws s3 sync $(QUETOO_DATA_S3_BUCKET)/ $(DATADIR)

release:
	aws s3 sync --delete $(INSTALL) $(QUETOO_S3_BUCKET)/$(BUILD)
	git rev-parse --short HEAD > $(TARGET)/revision
	aws s3 cp $(TARGET)/revision $(QUETOO_S3_BUCKET)/revisions/$(BUILD)

snapshot:
	tar -czvf $(SNAPSHOT) -C $(TARGET) quetoo

snapshot-release:
	aws s3 cp $(SNAPSHOT) $(QUETOO_S3_BUCKET)/snapshots/

clean:
	rm -rf $(TARGET)/*

.PHONY: all all-data configure compile pre-install install install-lib install-update install-data release snapshot snapshot-release clean
