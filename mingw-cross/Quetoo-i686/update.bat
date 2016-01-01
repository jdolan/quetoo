bin\rsync.exe --exclude *cygwin* --exclude *rsync* --delete -rkzhP --perms --chmod=a=rwx,Da+x --skip-compress=pk3 --stats rsync://quetoo.org/quetoo-mingw/i686/ .
