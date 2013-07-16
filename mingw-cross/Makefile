# Makefile for Quake2World.zip, requires Mingw Cross Environment http://mxe.cc/

MINGW_ARCH = i686

DATA = ../../quake2world-data
TARGET = target
QUAKE2WORLD = $(TARGET)/Quake2World
BINARIES = $(QUAKE2WORLD)/bin
LIBRARIES = $(QUAKE2WORLD)/lib
SHARE = $(QUAKE2WORLD)/share
REMOTE_USER = $(shell whoami)
RSYNC_REPOSITORY = quake2world.net:/opt/rsync/quake2world-win32/$(MINGW_ARCH)
RSYNC_TARGET = $(REMOTE_USER)@$(RSYNC_REPOSITORY)
IMAGE = $(TARGET)/Quake2World-BETA-$(MINGW_ARCH).zip
HTTP_REPOSITORY = quake2world.net:/var/www/quake2world/files
HTTP_TARGET = $(REMOTE_USER)@$(HTTP_REPOSITORY)

all: bundle

pre-install:
	install -d $(TARGET)
	cp -r $(MINGW_ARCH) $(QUAKE2WORLD)

install: pre-install
	make -C .. DESTDIR=$(realpath $(QUAKE2WORLD)) install

install-data: pre-install
	cp -r $(DATA)/target/* $(SHARE)

bundle: install
	./dllbundler.sh $(BINARIES)/quake2world.exe

release:
	rsync -avzP --delete $(QUAKE2WORLD)/*.* $(BINARIES) $(LIBRARIES) $(RSYNC_TARGET)

image:
	ln -s $(QUAKE2WORLD)
	zip -9 -r $(IMAGE) Quake2World
	rm Quake2World

release-image:
	rsync -avzP $(IMAGE) $(HTTP_TARGET)

clean:
	rm -rf $(TARGET)/*

