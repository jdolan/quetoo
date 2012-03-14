# Makefile for Quake2World.app

APP = Quake2World.app
MACOS = $(APP)/Contents/MacOS
RESOURCES = $(APP)/Contents/Resources

# These are derived by otool -L and some grep.
LIBS = \
	libFLAC.8.dylib \
	libSDL-1.2.0.dylib \
	libSDL_image-1.2.0.dylib \
	libSDL_mixer-1.2.0.dylib \
	libX11.6.dylib \
	libXau.6.dylib \
	libXdmcp.6.dylib \
	libXext.6.dylib \
	libXrandr.2.dylib \
	libXrender.1.dylib \
	libcrypto.1.0.0.dylib \
	libcurl.4.dylib \
	libiconv.2.dylib \
	libidn.11.dylib \
	libintl.8.dylib \
	libjpeg.8.dylib \
	libmikmod.3.dylib \
	libncurses.5.dylib \
	libogg.0.dylib \
	libpng14.14.dylib \
	libsmpeg-0.4.0.dylib \
	libssl.1.0.0.dylib \
	libtiff.3.dylib \
	libvorbis.0.dylib \
	libvorbisfile.3.dylib \
	libxcb.1.dylib \
	libz.1.dylib

quake2world: ../src/main/quake2world
	install ../src/main/quake2world ${MACOS}/quake2world

%.dylib:
	install /opt/local/lib/$*.dylib $(MACOS)/$*.dylib
	install_name_tool -change /opt/local/lib/$*.dylib @executable_path/$*.dylib $(MACOS)/quake2world

all: quake2world $(LIBS)

clean:
	rm -f $(MACOS)/quake2world
	rm -f $(MACOS)/*.dylib