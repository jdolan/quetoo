# Makefile for Quake2World.app, requires http://macdylibbundler.sourceforge.net/

DATA = ../../../quake2world-data
TARGET = target
QUAKE2WORLD = $(TARGET)/Quake2World/Quake2World.app
UPDATE = $(TARGET)/Quake2World/Update.app
LIBRARIES = $(QUAKE2WORLD)/Contents/lib
BINARIES = $(QUAKE2WORLD)/Contents/MacOS
RESOURCES = $(QUAKE2WORLD)/Contents/Resources
RSYNC_REPOSITORY = quake2world.net:/opt/rsync/quake2world-apple/x86_64
DMG = $(TARGET)/Quake2World-BETA.dmg
VOLUME_NAME = "Quake2World BETA"
HTTP_REPOSITORY = quake2world.net:/var/www/quake2world/files

all: bundle

pre-install:
	install -d $(TARGET)
	cp -r Quake2World $(TARGET)
	find $(TARGET) -name .turd -delete
	./set-icon.sh icon.icns $(TARGET)/Quake2World

install: pre-install
	install ../src/tools/q2wmap/q2wmap $(BINARIES)/q2wmap
	install ../src/main/quake2world $(BINARIES)/quake2world
	install ../src/cgame/default/.libs/cgame.so $(BINARIES)/default/cgame.so
	install ../src/game/default/.libs/game.so $(BINARIES)/default/game.so

install-data:
	cp -r $(DATA)/target/* $(RESOURCES)

bundle: install
	dylibbundler -b \
		-x $(BINARIES)/q2wmap \
		-x $(BINARIES)/quake2world \
		-x $(BINARIES)/default/cgame.so \
		-x $(BINARIES)/default/game.so \
	-d $(LIBRARIES) -of -p @executable_path/../lib

release:
	rsync -rzhP --delete -e ssh $(BINARIES) $(LIBRARIES) $(RSYNC_REPOSITORY)

image:
	ln -f -s /Applications $(TARGET)/Applications
	hdiutil create $(DMG) -srcfolder $(TARGET) -volname $(VOLUME_NAME)
	rm $(TARGET)/Applications

release-image:
	scp $(DMG) $(HTTP_REPOSITORY)

clean:
	rm -rf $(TARGET)/*
