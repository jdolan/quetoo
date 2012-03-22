# Makefile for Quake2World Linux distributable

DATA = ../../../quake2world-data/trunk
TARGET = target
QUAKE2WORLD = $(TARGET)/quake2world
LIBRARIES = $(QUAKE2WORLD)/lib
BINARIES = $(QUAKE2WORLD)/bin
RESOURCES = $(QUAKE2WORLD)
RSYNC_REPOSITORY = quake2world.net:/opt/rsync/quake2world-linux/x86_64

all: bundle

pre-install:
	install -d $(TARGET)
	svn export --force quake2world $(QUAKE2WORLD)

install: pre-install
	install ../src/tools/pak/pak $(BINARIES)/pak
	install ../src/tools/q2wmap/q2wmap $(BINARIES)/q2wmap
	install ../src/main/quake2world $(BINARIES)/quake2world
	install ../src/cgame/default/.libs/cgame.so $(BINARIES)/default/cgame.so
	install ../src/cgame/default/.libs/game.so $(BINARIES)/default/game.so

install-data:
	svn export --force $(DATA)/target $(RESOURCES)

bundle: install
	# This is perhaps where we would bundle up all of our libs

release:
	scp -r $(BINARIES) $(LIBRARIES) $(RSYNC_REPOSITORY)

image:
	# Tar up $(QUAKE2WORLD)

clean:
	rm -rf $(TARGET)/*
