# Makefile for Quake2World Linux distributable

ARCH := $(shell gcc -v 2>&1|grep Target|cut -d\  -f2|cut -d\- -f1)

DATA = ../../quake2world-data
TARGET = target
QUAKE2WORLD = $(TARGET)/quake2world
LIBRARIES = $(QUAKE2WORLD)/lib
BINARIES = $(QUAKE2WORLD)/bin
SHARE = $(QUAKE2WORLD)/share
REMOTE_USER = $(shell whoami)
RSYNC_REPOSITORY = quake2world.net:/opt/rsync/quake2world-linux/$(ARCH)
RSYNC_TARGET = $(REMOTE_USER)@$(RSYNC_REPOSITORY)
IMAGE = $(TARGET)/quake2world-BETA-$(ARCH).tar.gz
HTTP_REPOSITORY = quake2world.net:/var/www/quake2world/files
HTTP_TARGET = $(REMOTE_USER)@$(HTTP_REPOSITORY)

all: install

pre-install:
	install -d $(TARGET)
	cp -r quake2world $(TARGET)
	find $(TARGET) -name .turd -delete

install: pre-install
	install ../src/tools/q2wmap/q2wmap $(BINARIES)/q2wmap
	install ../src/main/quake2world $(BINARIES)/quake2world
	install ../src/cgame/default/.libs/cgame.so $(LIBRARIES)/default/cgame.so
	install ../src/game/default/.libs/game.so $(LIBRARIES)/default/game.so

install-data: pre-install
	cp -r $(DATA)/target/* $(SHARE)

release:
	rsync -rzhP --delete -e ssh $(BINARIES) $(LIBRARIES) $(RSYNC_TARGET)

image:
	tar -czvf $(IMAGE) -C $(TARGET) quake2world

release-image:
	scp $(IMAGE) $(HTTP_TARGET)

clean:
	rm -rf $(TARGET)/*
