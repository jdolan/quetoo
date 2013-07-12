#!/bin/bash -x

source ./common.sh

init_chroot

echo
echo "MINGW_TARGET: ${MINGW_TARGET}"
echo "MINGW_ARCH: ${MINGW_ARCH}"
echo

#
# Recursively finds and prints the .dll names required by the specified object.
#
function find_dll(){
	for i in `/usr/bin/${MINGW_ARCH}-w64-mingw32-objdump -p "$1" | grep "DLL Name:" | cut -d\: -f2 | cut -d\ -f2 | sort | uniq`; do
		dll=`find /usr/${MINGW_ARCH}-w64-mingw32 2>/dev/null | grep "$i" | grep -v .dll.a`
		echo $dll
		find_dll $dll
	done
}

#
# Creates snapshots and SCP's them, along with the updated build artifacts, to
# the project website / rsync repository.
#
function do_release() {
	cp `find_dll /tmp/quake2world-${MINGW_TARGET}/bin/quake2world.exe | sort | uniq | grep "\n"` /tmp/quake2world-${MINGW_TARGET}/bin

	find /tmp/quake2world-${MINGW_TARGET} -name "*.la" -delete
	find /tmp/quake2world-${MINGW_TARGET} -name "*.dll.a" -delete
	
	cp -r mingw-cross/${MINGW_ARCH}/bin ${MINGW_ARCH}/*.bat /tmp/quake2world-${MINGW_TARGET}
	
	cd /tmp/quake2world-${MINGW_TARGET}
	rsync -avzP *.bat bin lib maci@quake2world.net:/opt/rsync/quake2world-win32/${MINGW_ARCH}
	
	mkdir quake2world
	mv *.bat bin lib quake2world
	zip -9 -r quake2world-BETA-${MINGW_ARCH}.zip quake2world
	rsync -avzP quake2world-BETA-${MINGW_ARCH}.zip maci@quake2world.net:/var/www/quake2world/files/quake2world-BETA-${MINGW_ARCH}.zip
}

#
# Copy the SSH config into the chroot in case this is a release build
#
/usr/bin/mock -r ${CHROOT} --copyin ~/.ssh "/root/.ssh"

#
# Now do the build
#
/usr/bin/mock -r ${CHROOT} --shell "
	set -x
	export PATH=/usr/${MINGW_ARCH}-w64-mingw32/sys-root/mingw/bin:${PATH}
	cd /tmp/quake2world
	autoreconf -i --force
	./configure --host=${MINGW_ARCH}-w64-mingw32
	make
	make DESTDIR=/tmp/quake2world-${MINGW_TARGET} install
	
	if [[ \"$JOB_NAME\" == \"*release*\" ]]; then
		do_release
	fi
"

archive_workspace 
destroy_chroot
