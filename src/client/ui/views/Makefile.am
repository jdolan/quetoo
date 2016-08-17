
noinst_HEADERS = \
	BindTextView.h \
	CvarCheckbox.h \
	CvarSelect.h \
	CvarSlider.h \
	PlayerModelView.h \
	PrimaryButton.h \
	ServersTableView.h \
	SkinSelect.h

noinst_LTLIBRARIES = \
	libviews.la

libviews_la_SOURCES = \
	BindTextView.c \
	CvarCheckbox.c \
	CvarSelect.c \
	CvarSlider.c \
	CvarTextView.c \
	PlayerModelView.c \
	PrimaryButton.c \
	ServersTableView.c \
	SkinSelect.c

libviews_la_CFLAGS = \
	-I.. -I../.. -I../../.. \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@ \
	@OBJECTIVELYMVC_CFLAGS@

libviews_la_LDFLAGS = \
	-shared

libviews_la_LIBADD = \
	../../../libcommon.la \
	@OBJECTIVELYMVC_LIBS@
