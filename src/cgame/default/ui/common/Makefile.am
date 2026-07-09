cguicommondir = @PKGLIBDIR@/default/ui/common
cguicommon_DATA = \
	DialogViewController.css \
	DialogViewController.json \
	common.css

noinst_HEADERS = \
	BindTextView.h \
	CvarCheckbox.h \
	CvarSelect.h \
	CvarSlider.h \
	CvarTextView.h \
	DialogViewController.h \
	KeyValueTableView.h \
	KeyValueView.h \
	Scrollbar.h \
	ScrollThumb.h

noinst_LTLIBRARIES = \
	libcguicommon.la

libcguicommon_la_SOURCES = \
	BindTextView.c \
	CvarCheckbox.c \
	CvarSelect.c \
	CvarSlider.c \
	CvarTextView.c \
	DialogViewController.c \
	KeyValueTableView.c \
	KeyValueView.c \
	Scrollbar.c \
	ScrollThumb.c

libcguicommon_la_CFLAGS = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/cgame \
	-I$(top_srcdir)/src/cgame/default \
	-I$(top_srcdir)/src/cgame/default/ui \
	@BASE_CFLAGS@ \
	@OBJECTIVELYMVC_CFLAGS@ \
	@OPENAL_CFLAGS@ \
	@SDL3_CFLAGS@ \
	@SNDFILE_CFLAGS@

libcguicommon_la_LDFLAGS = \
	-shared

libcguicommon_la_LIBADD = \
	@OBJECTIVELYMVC_LIBS@
