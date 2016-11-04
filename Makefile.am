ACLOCAL_AMFLAGS = -I m4

SUBDIRS = \
	deps \
	src

RSYNC_REPOSITORY = \
	rsync://quetoo.org/quetoo

rsync-data:
	mkdir -p $(DESTDIR)@PKGDATADIR@
	rsync -rzhP --delete $(RSYNC_REPOSITORY) $(DESTDIR)@PKGDATADIR@

html:
	doxygen

html-clean:
	rm -rf doc/html

html-release:
	rsync -rz --delete doc/html/ jaydolan.com:/var/www/projects/quetoo

style:
	astyle -r "src/*.c" "src/*.h" \
		--add-brackets \
		--align-pointer=name \
		--attach-classes \
		--attach-extern-c \
		--attach-inlines \
		--attach-namespaces \
		--indent-cases \
		--indent-preproc-block \
		--indent-preproc-define
		--indent=tab \
		--pad-header \
		--pad-oper \
		--max-code-length=100 \
		--style=java
