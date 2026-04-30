bin_PROGRAMS = \
	quetoo \
	quetoo-dedicated

quetoo_SOURCES = \
	main.c
quetoo_CFLAGS = \
	-I$(top_srcdir)/src \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@ \
	@OPENAL_CFLAGS@ \
	@SDL3_CFLAGS@
quetoo_LDADD = \
	$(top_builddir)/src/client/libclient.la \
	$(top_builddir)/src/server/libserver.la

quetoo_dedicated_SOURCES = \
	main.c
quetoo_dedicated_CFLAGS = \
	-I$(top_srcdir)/src \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@ \
	@OPENAL_CFLAGS@ \
	@SDL3_CFLAGS@
quetoo_dedicated_LDADD = \
	$(top_builddir)/src/client/libclient_null.la \
	$(top_builddir)/src/server/libserver.la

if APPLE
all-local:
	@test -f quetoo && dsymutil quetoo -o quetoo.dSYM || true
	@test -f quetoo-dedicated && dsymutil quetoo-dedicated -o quetoo-dedicated.dSYM || true

install-exec-hook:
	cp -Rf quetoo.dSYM $(DESTDIR)$(bindir)/quetoo.dSYM
	cp -Rf quetoo-dedicated.dSYM $(DESTDIR)$(bindir)/quetoo-dedicated.dSYM
endif
