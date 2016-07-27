
noinst_HEADERS = \
	BindTextView.h \
	CvarCheckbox.h \
	CvarSlider.h \
	MenuInput.h \
	PrimaryButton.h

noinst_LTLIBRARIES = \
	libviews.la

libviews_la_SOURCES = \
	BindTextView.c \
	CvarCheckbox.c \
	CvarSlider.c \
	CvarTextView.c \
	MenuInput.c \
	PrimaryButton.c \
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
