#!/bin/bash
# this requires a publickey in /home/`whoami`/.ssh and a "config" file
# which contains something like:
# User maci


mock -r epel-6-i386 --clean
mock -r epel-6-i386 --rebuild quake2world-*.src.rpm &

while [ ! -e /var/lib/mock/epel-6-i386/root/builddir/build/BUILD/quake2world-0.0.1/master.zip ]; do
#  Sleep until file does exists/is created
  sleep 1
done

cp -aR /home/`whoami`/.ssh  /var/lib/mock/epel-6-i386/root/builddir/.ssh
chown -R `whoami`:mock /var/lib/mock/epel-6-i386/root/builddir/.ssh/





mock -r epel-6-x86_64 --clean
mock -r epel-6-x86_64 --rebuild quake2world-*.src.rpm &

while [ ! -e /var/lib/mock/epel-6-x86_64/root/builddir/build/BUILD/quake2world-0.0.1/master.zip ]; do
#  Sleep until file does exists/is created
  sleep 1
done

cp -aR /home/`whoami`/.ssh  /var/lib/mock/epel-6-x86_64/root/builddir/.ssh
chown -R `whoami`:mock /var/lib/mock/epel-6-x86_64/root/builddir/.ssh/


