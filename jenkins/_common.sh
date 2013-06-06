#!/bin/bash

set -e
set -x

CHROOT=`echo $JOB_NAME|cut -d\-  -f3-10`

if ([ "${CHROOT}" == "mingw64" ] || [ "${CHROOT}" == "mingw32" ])
then
	MINGW_TARGET=${CHROOT}
	CHROOT="fedora-18-x86_64"
	DEPS="openssh-clients rsync ${MINGW_TARGET}-SDL ${MINGW_TARGET}-SDL_image ${MINGW_TARGET}-SDL_mixer ${MINGW_TARGET}-curl ${MINGW_TARGET}-physfs ${MINGW_TARGET}-glib2 ${MINGW_TARGET}-libjpeg-turbo libtool ${MINGW_TARGET}-zlib ${MINGW_TARGET}-pkg-config ${MINGW_TARGET}-pdcurses http://maci.satgnu.net/rpmbuild/RPMS/noarch/${MINGW_TARGET}-physfs-2.0.3-3.fc18.noarch.rpm http://maci.satgnu.net/rpmbuild/RPMS/${MINGW_TARGET}lzma-sdk457-4.57-2.fc18.noarch.rpm"
else
	DEPS="openssh-clients rsync SDL-devel SDL_image-devel SDL_mixer-devel curl-devel physfs-devel glib2-devel libjpeg-turbo-devel libtool zlib-devel ncurses-devel check check-devel"
fi


function init_chroot() {
	/usr/bin/mock -r ${CHROOT} --clean
	/usr/bin/mock -r ${CHROOT} --init
	/usr/bin/mock -r ${CHROOT} --install ${DEPS}
	/usr/bin/mock -r ${CHROOT} --copyin ${WORKSPACE} "/tmp/quake2world"

}

function destroy_chroot() {
	/usr/bin/mock -r ${CHROOT} --clean
}

function archive_workspace() {
	rm -Rf ${WORKSPACE}/jenkins-quake2world*
	cp -r /var/lib/mock/${CHROOT}/root/tmp/quake2world-* "${WORKSPACE}/${BUILD_TAG}"
	cd ${WORKSPACE}
	tar czf ${BUILD_TAG}.tgz ${BUILD_TAG}
}
