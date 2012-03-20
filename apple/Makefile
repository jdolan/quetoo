# Makefile for Quake2World.app, requires http://macdylibbundler.sourceforge.net/

DATA = ../../../quake2world-data/trunk
TARGET = target
QUAKE2WORLD = $(TARGET)/Quake2World/Quake2World.app
UPDATE = $(TARGET)/Quake2World/Update.app
LIBRARIES = $(QUAKE2WORLD)/Contents/Libraries
BINARIES = $(QUAKE2WORLD)/Contents/MacOS
RESOURCES = $(QUAKE2WORLD)/Contents/Resources
RSYNC_REPOSITORY = quake2world.net:/opt/rsync/quake2world-apple/x86_64
DMG = $(TARGET)/Quake2World-BETA.dmg
VOLUME_NAME = "Quake2World BETA"

all: bundle

pre-install:
	install -d $(QUAKE2WORLD)
	svn export --force Quake2World.app $(QUAKE2WORLD)
	install -d $(UPDATE)
	svn export --force Update.app $(UPDATE)

install: pre-install
	install ../src/tools/pak/pak $(BINARIES)/pak
	install ../src/tools/q2wmap/q2wmap $(BINARIES)/q2wmap
	install ../src/main/quake2world $(BINARIES)/quake2world
	install ../src/cgame/default/.libs/cgame.so $(BINARIES)/default/cgame.so
	install ../src/game/default/.libs/game.so $(BINARIES)/default/game.so

install-data:
	svn export --force $(DATA)/target $(RESOURCES)

bundle: install
	dylibbundler -b \
		-x $(BINARIES)/pak \
		-x $(BINARIES)/q2wmap \
		-x $(BINARIES)/quake2world \
		-x $(BINARIES)/default/cgame.so \
		-x $(BINARIES)/default/game.so \
	-d $(LIBRARIES) -of -p @executable_path/../Libraries

release:
	scp -r $(BINARIES) $(LIBRARIES) $(RSYNC_REPOSITORY)

image:
	ln -f -s /Applications $(TARGET)/Applications
	hdiutil create $(DMG) -srcfolder $(TARGET) -volname $(VOLUME_NAME) 

clean:
	rm -rf $(TARGET)/*
