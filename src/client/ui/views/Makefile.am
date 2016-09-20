
noinst_HEADERS = \
	BindTextView.h \
	CrosshairView.h \
	CvarCheckbox.h \
	CvarSelect.h \
	CvarSlider.h \
	MapListCollectionItemView.h \
	MapListCollectionView.h \
	PlayerModelView.h \
	PrimaryButton.h \
	ServersTableView.h \
	SkinSelect.h \
	VideoModeSelect.h

noinst_LTLIBRARIES = \
	libviews.la

libviews_la_SOURCES = \
	BindTextView.c \
	CrosshairView.c \
	CvarCheckbox.c \
	CvarSelect.c \
	CvarSlider.c \
	CvarTextView.c \
	MapListCollectionItemView.c \
	MapListCollectionView.c \
	PlayerModelView.c \
	PrimaryButton.c \
	ServersTableView.c \
	SkinSelect.c \
	VideoModeSelect.c

libviews_la_CFLAGS = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/client \
	@BASE_CFLAGS@ \
	-Wno-unknown-pragmas \
	-Wno-unused-parameter \
	@GLIB_CFLAGS@ \
	@OBJECTIVELYMVC_CFLAGS@

libviews_la_LDFLAGS = \
	-shared

libviews_la_LIBADD = \
	../../../libcommon.la \
	@OBJECTIVELYMVC_LIBS@
