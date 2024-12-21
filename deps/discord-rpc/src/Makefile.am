
noinst_LTLIBRARIES = \
	libdiscord-rpc.la

noinst_HEADERS = \
	backoff.h \
	connection.h \
	msg_queue.h \
	rpc_connection.h \
	serialization.h

AM_CXXFLAGS = -std=c++11

libdiscord_rpc_la_SOURCES = \
	discord_rpc.cpp \
	rpc_connection.cpp \
	serialization.cpp
libdiscord_rpc_la_CPPFLAGS = \
	-I$(top_srcdir)/deps/discord-rpc/include \
	-I$(top_srcdir)/deps/rapidjson

if APPLE
libdiscord_rpc_la_SOURCES += \
	connection_unix.cpp \
	discord_register_osx.m
endif

if BSD
libdiscord_rpc_la_SOURCES += \
	connection_unix.cpp \
	discord_register_linux.cpp
endif

if MINGW
libdiscord_rpc_la_SOURCES += \
	connection_win.cpp \
	discord_register_win.cpp
endif

if LINUX
libdiscord_rpc_la_SOURCES += \
	connection_unix.cpp \
	discord_register_linux.cpp
endif
