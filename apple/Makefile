# Makefile for Quake2World.app, requires http://macdylibbundler.sourceforge.net/

APP = Quake2World.app
LIBRARIES = $(APP)/Contents/Libraries
MACOS = $(APP)/Contents/MacOS
RESOURCES = $(APP)/Contents/Resources

all: install bundle

install:
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
	
clean:
	rm -f $(MACOS)/pak
	rm -f $(MACOS)/q2wmap
	rm -f $(MACOS)/quake2world
	rm -f $(MACOS)/default/*.so
	rm -f $(LIBRARIES)/*.dylib