ACLOCAL_AMFLAGS = -I m4

SUBDIRS = \
	deps \
	doc \
	src

RSYNC_REPOSITORY = \
	rsync://jdolan.dyndns.org/quake2world

rsync-data:
	mkdir -p $(DESTDIR)@PKGDATADIR@
	rsync -rzhP --delete $(RSYNC_REPOSITORY) $(DESTDIR)@PKGDATADIR@
