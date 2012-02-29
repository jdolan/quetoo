ACLOCAL_AMFLAGS = -I m4

SUBDIRS = \
	deps \
	doc \
	src

RSYNC_REPOSITORY = \
	rsync://jdolan.dyndns.org/quake2world

rsync-data:
	rsync -rvz --delete $(RSYNC_REPOSITORY) @PKGDATADIR@
