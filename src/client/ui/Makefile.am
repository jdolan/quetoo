noinst_HEADERS = \
	ui.h \
	ui_context.h \
	ui_local.h \
	ui_main.h \
	\
	viewcontrollers/MainViewController.h \
	viewcontrollers/MenuViewController.h \
	\
	views/BindInput.h \
	views/VariableCheckboxInput.h \
	views/VariableSliderInput.h

noinst_LTLIBRARIES = \
	libui.la

libui_la_SOURCES = \
	ui_context.c \
	ui_main.c \
	\
	viewcontrollers/ControlsViewController.c \
	viewcontrollers/MainViewController.c \
	viewcontrollers/MenuViewController.c \
	\
	views/BindInput.c \
	views/VariableCheckboxInput.c \
	views/VariableSliderInput.c

libui_la_CFLAGS = \
	-I.. -I../.. \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@ \
	@OBJECTIVELYMVC_CFLAGS@

libui_la_LDFLAGS = \
	-shared

libui_la_LIBADD = \
	../../libcommon.la \
	@OBJECTIVELYMVC_LIBS@
