# Makefile for Quetoo.app, requires awscli and dylibbundler.
# http://macdylibbundler.sourceforge.net/

ARCH = x86_64
HOST = $(ARCH)-apple-darwin
UPDATE = quetoo-installer-small.jar
TARGET = target
INSTALL = $(TARGET)/Quetoo
QUETOO = $(INSTALL)/Quetoo.app
BINDIR = $(QUETOO)/Contents/MacOS
LIBDIR = $(QUETOO)/Contents/MacOS/lib
DATADIR = $(QUETOO)/Contents/Resources
SNAPSHOT = $(TARGET)/Quetoo-BETA-$(HOST).dmg
VOLUME_NAME = "Quetoo BETA $(HOST)"
QUETOO_S3_BUCKET = s3://quetoo
QUETOO_DATA_S3_BUCKET = s3://quetoo-data

all: clean install-lib install-update post-install snapshot

all-data: clean install-lib install-update install-data post-install snapshot

configure:
	cd .. ; autoreconf -i; ./configure CFLAGS='-g -O0' --prefix=/
	
compile: configure
	make -C .. -j8 all

pre-install:
	install -d $(INSTALL)
	cp -R Quetoo/* $(INSTALL)
	find $(INSTALL) -name .turd -delete

install: compile pre-install
	ln -f -s Contents/MacOS $(QUETOO)/bin
	ln -f -s Contents/MacOS/lib $(QUETOO)/lib
	ln -f -s Contents/Resources $(QUETOO)/share
	make -C .. DESTDIR=$(realpath $(QUETOO)) install
	rm -f $(QUETOO)/bin $(QUETOO)/lib $(QUETOO)/share

install-lib: install
	dylibbundler -b \
		-x $(BINDIR)/quemap \
		-x $(BINDIR)/quetoo \
		-x $(BINDIR)/quetoo-dedicated \
		-x $(BINDIR)/quetoo-master \
		-x $(BINDIR)/quetoo-update \
		-x $(LIBDIR)/default/ai.so \
		-x $(LIBDIR)/default/cgame.so \
		-x $(LIBDIR)/default/game.so \
	-d $(LIBDIR) -of -p @executable_path/lib

install-update:
	aws s3 cp $(QUETOO_S3_BUCKET)/snapshots/$(UPDATE) $(LIBDIR)/

install-data:
	aws s3 sync $(QUETOO_DATA_S3_BUCKET)/ $(DATADIR)

post-install:
	./set-icon.sh $(DATADIR)/quetoo-folder.icns $(INSTALL)
	./set-icon.sh $(DATADIR)/quemap.icns $(BINDIR)/quemap
	./set-icon.sh $(DATADIR)/quetoo.icns $(BINDIR)/quetoo
	./set-icon.sh $(DATADIR)/quetoo-dedicated.icns $(BINDIR)/quetoo-dedicated
	./set-icon.sh $(DATADIR)/quetoo-update.icns $(BINDIR)/quetoo-update

release:
	aws s3 sync --exclude 'Icon*' --delete $(INSTALL) $(QUETOO_S3_BUCKET)/$(HOST)

snapshot:
	ln -f -s /Applications $(TARGET)/Applications
	hdiutil create -ov $(SNAPSHOT) -srcfolder $(TARGET) -volname $(VOLUME_NAME)
	rm -f $(TARGET)/Applications

snapshot-release:
	aws s3 cp $(SNAPSHOT) $(QUETOO_S3_BUCKET)/snapshots/

clean:
	rm -rf $(TARGET)/*

.PHONY: all all-data configure compile pre-install install install-lib install-update install-data post-install release snapshot snapshot-release clean
