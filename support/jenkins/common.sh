# Common functions for all Jenkins build scripts

CHROOT=$(echo "${JOB_NAME}" | cut -d\- -f3)

if ([ "${CHROOT}" == "mingw64" ] || [ "${CHROOT}" == "mingw32" ]); then
	
	export MINGW_TARGET=${CHROOT}
	if [ "${MINGW_TARGET}" == "mingw64" ]; then
		export MINGW_HOST="x86_64-w64-mingw32"
		export MINGW_ARCH="x86_64"
	else
		export MINGW_HOST="i686-w64-mingw32"
		export MINGW_ARCH="i686"
	fi
	
	CHROOT="fedora-18-x86_64"
	
	DEPS="openssh-clients \
		rsync \
		${MINGW_TARGET}-SDL ${MINGW_TARGET}-SDL_image ${MINGW_TARGET}-SDL_mixer \
		${MINGW_TARGET}-curl \
		${MINGW_TARGET}-physfs \
		${MINGW_TARGET}-glib2 \
		${MINGW_TARGET}-libjpeg-turbo libtool \
		${MINGW_TARGET}-zlib \
		${MINGW_TARGET}-pkg-config \
		${MINGW_TARGET}-pdcurses \
		http://maci.satgnu.net/rpmbuild/RPMS/${MINGW_TARGET}-physfs-2.0.3-3.fc18.noarch.rpm \
		http://maci.satgnu.net/rpmbuild/RPMS/${MINGW_TARGET}-lzma-sdk457-4.57-2.fc18.noarch.rpm
		"
else
	DEPS="openssh-clients \
		rsync \
		SDL-devel SDL_image-devel SDL_mixer-devel \
		curl-devel \
		physfs-devel \
		glib2-devel \
		libjpeg-turbo-devel \
		libtool \
		zlib-devel \
		ncurses-devel \
		check \
		check-devel
		"
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
