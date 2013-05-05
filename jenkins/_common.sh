#!/bin/bash

set -e

ENV=`echo $JOB_NAME|cut -d\-  -f3-10`

function setup_fedora17(){
DEPS="SDL-devel SDL_image-devel SDL_mixer-devel curl-devel physfs-devel glib2-devel libjpeg-turbo-devel libtool zlib-devel ncurses-devel check check-devel"

/usr/bin/mock -r ${ENV} --clean
/usr/bin/mock -r ${ENV} --init
/usr/bin/mock -r ${ENV} --install ${DEPS}
/usr/bin/mock -r ${ENV} --copyin . "/tmp/quake2world"
}

function destroy_fedora17() {
	
	/usr/bin/mock -r ${ENV} --clean
	
}
