# Makefile for Quetoo Windows distributable, requires awscli.
# export PATH=/usr/x86_64-w64-mingw32/sys-root/mingw/bin:${PATH}

ARCH = x86_64
HOST = $(ARCH)-w64-mingw32
UPDATE = quetoo-installer-small.jar
TARGET = target
INSTALL = $(TARGET)/Quetoo
BINDIR = $(INSTALL)/bin
ETCDIR = $(INSTALL)/etc
LIBDIR = $(INSTALL)/lib
DATADIR = $(INSTALL)/share
SNAPSHOT = $(TARGET)/Quetoo-BETA-$(HOST).zip
QUETOO_S3_BUCKET = s3://quetoo
QUETOO_DATA_S3_BUCKET = s3://quetoo-data

all: clean install-lib install-update snapshot

all-data: clean install-lib install-update install-data snapshot

configure:
	cd .. ; autoreconf -i; ./configure CFLAGS='-g -O0' --host=$(HOST) --prefix=/
	
compile: configure
	make -C .. all

pre-install:
	install -d $(INSTALL)
	cp -r Quetoo-$(ARCH)/* $(INSTALL)
	find $(INSTALL) -name .turd -delete

install: compile pre-install
	make -C .. DESTDIR=$(realpath $(INSTALL)) install

install-lib: install
	./dllbundler.sh -h $(HOST) \
		$(BINDIR)/quemap.exe \
		$(BINDIR)/quetoo.exe \
		$(BINDIR)/quetoo-dedicated.exe \
		$(BINDIR)/quetoo-master.exe \
		$(BINDIR)/quetoo-update.exe

install-update:
	aws s3 cp $(QUETOO_S3_BUCKET)/snapshots/$(UPDATE) $(LIBDIR)/

install-data:
	aws s3 sync $(QUETOO_DATA_S3_BUCKET)/ $(DATADIR)

release:
	aws s3 sync --delete $(INSTALL) $(QUETOO_S3_BUCKET)/$(HOST)

snapshot:
	rm -f Quetoo; ln -s $(INSTALL)
	zip -9 -r $(SNAPSHOT) Quetoo

snapshot-release:
	aws s3 cp $(SNAPSHOT) $(QUETOO_S3_BUCKET)/snapshots/

clean:
	rm -rf $(TARGET)/*

.PHONY: all all-data configure compile pre-install install install-lib install-data release snapshot snapshot-release clean
