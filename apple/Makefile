# Makefile for Quake2World.app, requires http://macdylibbundler.sourceforge.net/

TARGET = target
APP = $(TARGET)/Quake2World.app
DATA = ../../../quake2world-data/trunk
LIBRARIES = $(APP)/Contents/Libraries
MACOS = $(APP)/Contents/MacOS
RESOURCES = $(APP)/Contents/Resources
DMG = $(TARGET)/Quake2World-BETA.dmg
VOLUME_NAME = "Quake2World BETA"

all: bundle image

pre-install:
	install -d $(APP)
	svn export --force Quake2World.app $(APP)
	svn export --force $(DATA)/target $(RESOURCES)

install: pre-install
	install ../src/tools/pak/pak $(MACOS)/pak
	install ../src/tools/q2wmap/q2wmap $(MACOS)/q2wmap
	install ../src/main/quake2world $(MACOS)/quake2world
	install ../src/cgame/default/.libs/cgame.so $(MACOS)/default/cgame.so
	install ../src/game/default/.libs/game.so $(MACOS)/default/game.so

bundle: install
	dylibbundler -b \
		-x $(MACOS)/pak \
		-x $(MACOS)/q2wmap \
		-x $(MACOS)/quake2world \
		-x $(MACOS)/default/cgame.so \
		-x $(MACOS)/default/game.so \
	-d $(LIBRARIES) -of -p @executable_path/../Libraries
	
image:
	ln -f -s /Applications $(TARGET)/Applications
	hdiutil create $(DMG) -srcfolder $(TARGET) -volname $(VOLUME_NAME) 
	
clean:
	rm -rf $(TARGET)/*
