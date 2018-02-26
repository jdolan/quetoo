# Makefile for Quetoo Linux snapshotributable, requires awscli.

ARCH = x86_64
HOST = $(ARCH)-pc-linux
UPDATE = quetoo-installer-small.jar
TARGET = target
INSTALL = $(TARGET)/quetoo
BINDIR = $(INSTALL)/bin
LIBDIR = $(INSTALL)/lib
DATADIR = $(INSTALL)/share
SNAPSHOT = $(TARGET)/quetoo-BETA-$(HOST).tar.gz
QUETOO_S3_BUCKET = s3://quetoo
QUETOO_DATA_S3_BUCKET = s3://quetoo-data

all: clean install-lib install-update snapshot

all-data: clean install-lib install-update install-data snapshot

configure:
	cd .. ; autoreconf -i; ./configure CFLAGS='-g -O0' LDFLAGS='-Wl,-rpath=XORIGIN/../lib' --host=$(HOST) --prefix=/

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
		$(BINDIR)/quetoo-update \
		$(LIBDIR)/default/ai.so \
		$(LIBDIR)/default/cgame.so \
		$(LIBDIR)/default/game.so

install-update:
	aws s3 cp $(QUETOO_S3_BUCKET)/snapshots/$(UPDATE) $(LIBDIR)/

install-data:
	aws s3 sync $(QUETOO_DATA_S3_BUCKET)/ $(DATADIR)

release:
	aws s3 sync --delete $(INSTALL) $(QUETOO_S3_BUCKET)/$(HOST)

snapshot:
	tar -czvf $(SNAPSHOT) -C $(TARGET) quetoo

snapshot-release:
	aws s3 cp $(SNAPSHOT) $(QUETOO_S3_BUCKET)/snapshots/

clean:
	rm -rf $(TARGET)/*

.PHONY: all all-data configure compile pre-install install install-lib install-update install-data release snapshot snapshot-release clean
