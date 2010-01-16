ACLOCAL_AMFLAGS = -I m4

SUBDIRS = \
	src \
	doc

RSYNC_REPOSITORY = \
	rsync://jdolan.dyndns.org/quake2world

rsync-data:
	rsync -rvz --delete $(RSYNC_REPOSITORY) @PKGDATADIR@
