# Makefile for Quetoo Linux distributable, requires awscli.

ARCH = x86_64
HOST = $(ARCH)-pc-linux
DATA = ../../quetoo-data
ARTIFACT = quetoo
TARGET = target
INSTALL = $(TARGET)/$(ARTIFACT)
LIBDIR = $(INSTALL)/lib
BINDIR = $(INSTALL)/bin
DATADIR = $(INSTALL)/share
DIST = $(TARGET)/$(ARTIFACT)-BETA-$(ARCH).tar.gz
S3_BUCKET = s3://quetoo

all: clean install-lib dist

all-data: clean install-lib install-data dist

configure:
	cd .. ; autoreconf -i; ./configure \
		LDFLAGS='-Wl,-rpath=XORIGIN/../lib' \
		--host=$(HOST) \
		--prefix=/ \
		--with-tools='master quemap update'
	
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
		$(BINDIR)/quemap \
		$(BINDIR)/quetoo \
		$(BINDIR)/quetoo-dedicated \
		$(BINDIR)/quetoo-master \
		$(BINDIR)/quetoo-update

install-data:
	cp -r $(DATA)/target/* $(DATADIR)

release:
	aws s3 sync $(INSTALL) $(S3_BUCKET)/$(HOST)

dist:
	tar -czvf $(DIST) -C $(TARGET) $(ARTIFACT)

dist-release:
	aws s3 cp $(DIST) $(S3_BUCKET)

clean:
	rm -rf $(TARGET)/*

.PHONY: all all-data configure compile pre-install install install-lib install-data release dist dist-release clean
