#!/bin/bash
mock -r epel-6-i386 --clean
mock -r epel-6-i386 --rebuild quake2world-*.src.rpm &

while [ ! -e /var/lib/mock/epel-6-i386/root/builddir/.subversion/config ]; do
# Sleep until file does exists/is created
sleep 1
done

cp -aR /home/maci/.ssh  /var/lib/mock/epel-6-i386/root/builddir/.ssh
chown -R maci:mock /var/lib/mock/epel-6-i386/root/builddir/.ssh/





mock -r epel-6-x86_64 --clean
mock -r epel-6-x86_64 --rebuild quake2world-*.src.rpm &

while [ ! -e /var/lib/mock/epel-6-x86_64/root/builddir/.subversion/config ]; do
# Sleep until file does exists/is created
sleep 1
done

cp -aR /home/maci/.ssh  /var/lib/mock/epel-6-x86_64/root/builddir/.ssh
chown -R maci:mock /var/lib/mock/epel-6-x86_64/root/builddir/.ssh/


