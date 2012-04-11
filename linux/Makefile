# Makefile for Quake2World Linux distributable

DATA = ../../../quake2world-data/trunk
TARGET = target
QUAKE2WORLD = $(TARGET)/quake2world
LIBRARIES = $(QUAKE2WORLD)/lib
BINARIES = $(QUAKE2WORLD)/bin
RESOURCES = $(QUAKE2WORLD)/share
RSYNC_REPOSITORY = quake2world.net:/opt/rsync/quake2world-linux/x86_64
HTTP_REPOSITORY = quake2world.net:/var/www/quake2world/files

all: install

pre-install:
	install -d $(TARGET)
	svn export --force quake2world $(QUAKE2WORLD)

install: pre-install
	install ../src/tools/pak/pak $(BINARIES)/pak
	install ../src/tools/q2wmap/q2wmap $(BINARIES)/q2wmap
	install ../src/main/quake2world $(BINARIES)/quake2world
	install ../src/cgame/default/.libs/cgame.so $(LIBRARIES)/default/cgame.so
	install ../src/game/default/.libs/game.so $(LIBRARIES)/default/game.so

install-data:
	svn export --force $(DATA)/target $(RESOURCES)

release: install
	scp -r $(BINARIES) $(LIBRARIES) $(RSYNC_REPOSITORY)

image: install
	tar -czvf $(TARGET)/quake2world-BETA-x86_64.tar.gz -C $(TARGET) quake2world

release-image: image
	scp $(TARGET)/quake2world-BETA-x86_64.tar.gz $(HTTP_REPOSITORY)

clean:
	rm -rf $(TARGET)/*
