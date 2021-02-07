noinst_HEADERS = \
	net.h \
	net_chan.h \
	net_http.h \
	net_message.h \
	net_tcp.h \
	net_udp.h

noinst_LTLIBRARIES = \
	libnet.la

libnet_la_SOURCES = \
	net.c \
	net_chan.c \
	net_http.c \
	net_message.c \
	net_tcp.c \
	net_udp.c

libnet_la_CFLAGS = \
	-I$(top_srcdir)/src \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@ \
	@OBJECTIVELY_CFLAGS@ \
	@SDL2_CFLAGS@

libnet_la_LDFLAGS = \
	-shared

libnet_la_LIBADD = \
	$(top_builddir)/src/common/libcommon.la \
	@OBJECTIVELY_LIBS@
