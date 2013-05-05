#!/bin/bash

set -e

ENV=`echo $JOB_NAME|cut -d\-  -f3-10`

function setup_fedora17(){
DEPS="SDL-devel SDL_image-devel SDL_mixer-devel curl-devel physfs-devel glib2-devel libjpeg-turbo-devel libtool zlib-devel ncurses-devel check check-devel"

/usr/bin/mock -r ${ENV} --clean
/usr/bin/mock -r ${ENV} --init
/usr/bin/mock -r ${ENV} --install ${DEPS}
/usr/bin/mock -r ${ENV} --copyin ${WORKSPACE} "/tmp/quake2world"
}

function destroy_fedora17() {
	
/usr/bin/mock -r ${ENV} --clean
	
}

function setup_mingw(){
TARGET="i686"
if [ ${ENV} == "mingw64" ]
then
	TARGET="x86_64"
fi

MINGW_ENV="fedora-18-x86_64"

MINGW_DEPS="${ENV}-SDL ${ENV}-SDL_image ${ENV}-SDL_mixer ${ENV}-curl ${ENV}-physfs ${ENV}-glib2 ${ENV}-libjpeg-turbo libtool ${ENV}-zlib ${ENV}-pkg-config ${ENV}-pdcurses"

/usr/bin/mock -r ${MINGW_ENV} --clean
/usr/bin/mock -r ${MINGW_ENV} --init
/usr/bin/mock -r ${MINGW_ENV} --install ${MINGW_DEPS} http://maci.satgnu.net/rpmbuild/RPMS/noarch/${ENV}-physfs-2.0.3-1.fc18.noarch.rpm
/usr/bin/mock -r ${MINGW_ENV} --copyin ${WORKSPACE} "/tmp/quake2world"
}

function destroy_mingw() {
	
/usr/bin/mock -r ${MINGW_ENV} --clean
	
}

function archive_workspace() {
rm -Rf ${WORKSPACE}/jenkins-quake2world*
/usr/bin/mock -r ${ENV} --copyout "/tmp/quake2world-${ENV}" "${WORKSPACE}/${BUILD_TAG}"
cd ${WORKSPACE}
tar czf ${BUILD_TAG}.tgz ${BUILD_TAG}
}
