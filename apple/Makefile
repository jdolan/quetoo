# Makefile for Quetoo.app, requires awscli and dylibbundler.
# http://macdylibbundler.sourceforge.net/

CODESIGN_IDENTITY = "77C0E36DFF54D135D716AD4CA752823838E4D648"

ARCH = x86_64
BUILD = $(ARCH)-apple-darwin
TARGET = target
INSTALL = $(TARGET)/Quetoo.app
BINDIR = $(INSTALL)/Contents/MacOS
LIBDIR = $(INSTALL)/Contents/MacOS/lib
DATADIR = $(INSTALL)/Contents/Resources
INSTALLER = quetoo-installer-small.jar
SNAPSHOT = $(TARGET)/Quetoo-BETA-$(BUILD).dmg
VOLUME_NAME = "Quetoo BETA $(BUILD)"
QUETOO_S3_BUCKET = s3://quetoo
QUETOO_DATA_S3_BUCKET = s3://quetoo-data

all: clean compile install install-lib install-installer post-install

configure:
	cd .. ; autoreconf -i; ./configure CFLAGS='-g -O2' --prefix=/
	
compile: configure
	make -C .. -j8 all

pre-install:
	install -d $(INSTALL)
	cp -r Quetoo.app/* $(INSTALL)
	find $(INSTALL) -name .turd -delete

install: pre-install
	ln -f -s Contents/MacOS $(INSTALL)/bin
	ln -f -s Contents/MacOS/lib $(INSTALL)/lib
	ln -f -s Contents/Resources $(INSTALL)/share
	make -C .. DESTDIR=$(realpath $(INSTALL)) install
	rm -f $(INSTALL)/bin $(INSTALL)/lib $(INSTALL)/share

install-lib:
	dylibbundler -b \
		-x $(BINDIR)/quemap \
		-x $(BINDIR)/quetoo \
		-x $(BINDIR)/quetoo-dedicated \
		-x $(BINDIR)/quetoo-master \
		-x $(LIBDIR)/default/ai.so \
		-x $(LIBDIR)/default/cgame.so \
		-x $(LIBDIR)/default/game.so \
	-d $(LIBDIR) -of -p @executable_path/lib

install-installer:
	aws s3 cp $(QUETOO_S3_BUCKET)/snapshots/$(INSTALLER) $(LIBDIR)/

post-install:
	xattr -cr $(QUETOO)
	codesign -s $(CODESIGN_IDENTITY) --deep --force $(INSTALL)

release:
	aws s3 sync --delete $(INSTALL) $(QUETOO_S3_BUCKET)/$(BUILD)
	git rev-parse --short HEAD > $(TARGET)/revision
	aws s3 cp $(TARGET)/revision $(QUETOO_S3_BUCKET)/revisions/$(BUILD)

install-data:
	aws s3 sync $(QUETOO_DATA_S3_BUCKET)/ $(DATADIR)

snapshot:
	ln -f -s /Applications $(TARGET)/Applications
	hdiutil create -ov $(SNAPSHOT) -srcfolder $(TARGET) -volname $(VOLUME_NAME)
	rm -f $(TARGET)/Applications

snapshot-release:
	aws s3 cp $(SNAPSHOT) $(QUETOO_S3_BUCKET)/snapshots/

clean:
	rm -rf $(TARGET)/*

.PHONY: all configure compile pre-install install install-lib install-installer post-install release install-data snapshot snapshot-release clean
