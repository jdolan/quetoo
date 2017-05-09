
noinst_HEADERS = \
	AudioView.h \
	BindTextView.h \
	CreateServerView.h \
	CrosshairView.h \
	CvarCheckbox.h \
	CvarSelect.h \
	CvarSlider.h \
	DialogView.h \
	HueSlider.h \
	InputView.h \
	InterfaceView.h \
	MapListCollectionItemView.h \
	MapListCollectionView.h \
	PlayerModelView.h \
	PrimaryButton.h \
	PrimaryIcon.h \
	QuickJoinView.h \
	ServerBrowserView.h \
	ServersTableView.h \
	SkinSelect.h \
	VideoModeSelect.h \
	VideoView.h

noinst_LTLIBRARIES = \
	libviews.la

libviews_la_SOURCES = \
	AudioView.c \
	BindTextView.c \
	CreateServerView.c \
	CrosshairView.c \
	CvarCheckbox.c \
	CvarSelect.c \
	CvarSlider.c \
	CvarTextView.c \
	DialogView.c \
	HueSlider.c \
	InputView.c \
	InterfaceView.c \
	MapListCollectionItemView.c \
	MapListCollectionView.c \
	PlayerModelView.c \
	PrimaryButton.c \
	PrimaryIcon.c \
	QuickJoinView.c \
	ServerBrowserView.c \
	ServersTableView.c \
	SkinSelect.c \
	VideoModeSelect.c \
	VideoView.c

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
