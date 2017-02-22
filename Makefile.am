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

style:
	astyle -r "src/*.c" "src/*.h" \
		--add-brackets \
		--align-pointer=name \
		--attach-classes \
		--attach-extern-c \
		--attach-inlines \
		--attach-namespaces \
		--break-after-logical \
		--indent-cases \
		--indent-preproc-block \
		--indent-preproc-define \
		--indent-switches \
		--indent=tab \
		--pad-header \
		--pad-oper \
		--max-code-length=120 \
		--style=java \
		--suffix=none
