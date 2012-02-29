noinst_HEADERS = \
	AntPerfTimer.h \
	LoadOGL.h \
	MiniSDL12.h \
	MiniSDL13.h \
	TwBar.h \
	TwColors.h \
	TwFonts.h \
	TwGraph.h \
	TwMgr.h \
	TwOpenGL.h \
	TwPrecomp.h \
	resource.h

noinst_LTLIBRARIES = \
	libanttweakbar.la
	
if BUILD_CLIENT

libanttweakbar_la_SOURCES = \
	LoadOGL.cpp \
	TwBar.cpp \
	TwColors.cpp \
	TwEventSDL.c \
	TwEventSDL12.c \
	TwEventSDL13.c \
	TwFonts.cpp \
	TwMgr.cpp \
	TwOpenGL.cpp \
	TwPrecomp.cpp

libanttweakbar_la_CXXFLAGS = \
	-I../include \
	-pipe \
	-ObjC++ \
	@ANTTWEAKBAR_CFLAGS@ \
	@OPENGL_CFLAGS@ \
	@SDL_CFLAGS@
	
libanttweakbar_la_CFLAGS = \
	-I../include \
	-pipe \
	@ANTTWEAKBAR_CFLAGS@ \
	@OPENGL_CFLAGS@ \
	@SDL_CFLAGS@

else

libanttweakbar_la_SOURCES = \
	../../src/null.c

libanttweakbar_la_CFLAGS = \
	-D__LIBTOOL_LIBRARY__=libanttweakbar_la

endif
	