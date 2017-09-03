
noinst_HEADERS = \
	BindTextView.h \
	CrosshairView.h \
	CvarCheckbox.h \
	CvarSelect.h \
	CvarSlider.h \
	DialogView.h \
	HueSlider.h \
	MainView.h \
	MapListCollectionItemView.h \
	MapListCollectionView.h \
	PlayerModelView.h \
	PrimaryButton.h \
	PrimaryIcon.h \
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
	DialogView.c \
	HueSlider.c \
	MainView.c \
	MapListCollectionItemView.c \
	MapListCollectionView.c \
	PlayerModelView.c \
	PrimaryButton.c \
	PrimaryIcon.c \
	SkinSelect.c \
	VideoModeSelect.c

libviews_la_CFLAGS = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/cgame \
	-I$(top_srcdir)/src/cgame/default \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@ \
	@OBJECTIVELYMVC_CFLAGS@ \
	@OPENGL_CFLAGS@

libviews_la_LDFLAGS = \
	-shared

libviews_la_LIBADD = \
	@OBJECTIVELYMVC_LIBS@ \
	@OPENGL_LIBS@
