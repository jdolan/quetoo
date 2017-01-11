# Makefile for Quetoo Windows distributable, requires awscli.
# export PATH=/usr/x86_64-w64-mingw32/sys-root/mingw/bin:${PATH}

ARCH = x86_64
HOST = $(ARCH)-w64-mingw32
DATA = ../../quetoo-data
TARGET = target
INSTALL = $(TARGET)/Quetoo
BINDIR = $(INSTALL)/bin
ETCDIR = $(INSTALL)/etc
LIBDIR = $(INSTALL)/lib
DATADIR = $(INSTALL)/share
DIST = $(TARGET)/Quetoo-BETA-$(ARCH).zip
S3_BUCKET = s3://quetoo

all: clean install-lib dist

all-data: clean install-lib install-data dist

configure:
	cd .. ; autoreconf -i; ./configure --host=$(HOST) --prefix=/
	
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
		$(BINDIR)/quetoo-update.exe

install-data:
	cp -r $(DATA)/target/* $(DATADIR)

release:
	aws s3 sync $(INSTALL) $(S3_BUCKET)/$(HOST)

dist:
	rm -f Quetoo; ln -s $(INSTALL)
	zip -9 -r $(DIST) Quetoo

dist-release:
	aws s3 cp $(DIST) $(S3_BUCKET)

clean:
	rm -rf $(TARGET)/*

.PHONY: all all-data configure compile pre-install install install-lib install-data release dist dist-release clean

