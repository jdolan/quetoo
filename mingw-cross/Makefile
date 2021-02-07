# Makefile for Quetoo Windows distributable, requires awscli.
# export PATH=/usr/x86_64-w64-mingw32/sys-root/mingw/bin:${PATH}

ARCH = x86_64
BUILD = $(ARCH)-w64-mingw32
TARGET = target
INSTALL = $(TARGET)/Quetoo
BINDIR = $(INSTALL)/bin
ETCDIR = $(INSTALL)/etc
LIBDIR = $(INSTALL)/lib
DATADIR = $(INSTALL)/share
INSTALLER = quetoo-installer-small.jar
SNAPSHOT = $(TARGET)/Quetoo-BETA-$(BUILD).zip
QUETOO_S3_BUCKET = s3://quetoo
QUETOO_DATA_S3_BUCKET = s3://quetoo-data

all: clean compile install install-lib install-installer install-data snapshot

configure:
	cd .. ; autoreconf -i; ./configure CFLAGS='-g -O2' --host=$(BUILD) --prefix=/
	
compile: configure
	make -C .. all

pre-install:
	install -d $(INSTALL)
	cp -r Quetoo-$(ARCH)/* $(INSTALL)
	find $(INSTALL) -name .turd -delete

install: pre-install
	make -C .. DESTDIR=$(realpath $(INSTALL)) install

install-lib:
	./dllbundler.sh -h $(BUILD) \
		$(BINDIR)/quemap.exe \
		$(BINDIR)/quetoo.exe \
		$(BINDIR)/quetoo-dedicated.exe \
		$(BINDIR)/quetoo-master.exe

install-installer:
	aws s3 cp $(QUETOO_S3_BUCKET)/snapshots/$(INSTALLER) $(LIBDIR)/

install-data:
	aws s3 sync $(QUETOO_DATA_S3_BUCKET)/ $(DATADIR)

release:
	aws s3 sync --delete $(INSTALL) $(QUETOO_S3_BUCKET)/$(BUILD)
	git rev-parse --short HEAD > $(TARGET)/revision
	aws s3 cp $(TARGET)/revision $(QUETOO_S3_BUCKET)/revisions/$(BUILD)

snapshot:
	rm -f Quetoo; ln -s $(INSTALL)
	zip -9 -r $(SNAPSHOT) Quetoo

snapshot-release:
	aws s3 cp $(SNAPSHOT) $(QUETOO_S3_BUCKET)/snapshots/

clean:
	rm -rf $(TARGET)/*

.PHONY: all configure compile pre-install install install-lib install-installer install-data release snapshot snapshot-release clean
