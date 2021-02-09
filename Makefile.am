ACLOCAL_AMFLAGS = -I m4

SUBDIRS = \
	deps \
	src

html:
	doxygen

html-clean:
	rm -rf doc/html

html-release:
	rsync -rz --delete doc/html/ jaydolan.com:/var/www/projects/quetoo
