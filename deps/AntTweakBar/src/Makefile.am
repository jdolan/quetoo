noinst_LTLIBRARIES = \
	libanttweakbar.la
	
if BUILD_CLIENT

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
	@BASE_CXXFLAGS@ \
	@ANTTWEAKBAR_CXXFLAGS@ \
	@OPENGL_CFLAGS@ \
	@SDL_CFLAGS@
	
libanttweakbar_la_CFLAGS = \
	-I../include \
	@BASE_CFLAGS@ \
	@ANTTWEAKBAR_CFLAGS@ \
	@OPENGL_CFLAGS@ \
	@SDL_CFLAGS@

libanttweakbar_la_LIBADD = \
	@BASE_LIBS@ \
	@ANTTWEAKBAR_LIBS@ \
	@OPENGL_LIBS@ \
	@SDL_LIBS@ \
	-lstdc++

else

libanttweakbar_la_SOURCES = \
	../../../src/null.c

libanttweakbar_la_CFLAGS = \
	-D__LIBTOOL_LIBRARY__=libanttweakbar_la

endif
	