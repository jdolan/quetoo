bin\rsync.exe --exclude *cygwin* --exclude *rsync* --delete -rkzhP --perms --chmod=a=rwx,Da+x rsync://quetoo.org/quetoo-mingw/i686/ .
