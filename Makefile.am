ACLOCAL_AMFLAGS = -I m4

SUBDIRS = \
	deps \
	src

RSYNC_REPOSITORY = \
	rsync://quake2world.net/quake2world

rsync-data:
	mkdir -p $(DESTDIR)@PKGDATADIR@
	rsync -rzhP --delete $(RSYNC_REPOSITORY) $(DESTDIR)@PKGDATADIR@
