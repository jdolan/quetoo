# Common functions for all Jenkins build scripts

OS=$(echo "${JOB_NAME}" | cut -d\- -f3)

echo
echo "OS: ${OS"
echo

if [[ "${OS}" == "mingw*" ]]; then
	
	if [ "${OS}" == "mingw64" ]; then
		export MINGW_HOST="x86_64-w64-mingw32"
		export MINGW_ARCH="x86_64"
	else
		export MINGW_HOST="i686-w64-mingw32"
		export MINGW_ARCH="i686"
	fi
	
	CHROOT="fedora-18-x86_64"
	
	DEPS="openssh-clients \
		rsync \
		${OS}-SDL ${OS}-SDL_image ${OS}-SDL_mixer \
		${OS}-curl \
		${OS}-physfs \
		${OS}-glib2 \
		${OS}-libjpeg-turbo libtool \
		${OS}-zlib \
		${OS}-pkg-config \
		${OS}-pdcurses \
		http://maci.satgnu.net/rpmbuild/RPMS/${OS}-physfs-2.0.3-3.fc18.noarch.rpm \
		http://maci.satgnu.net/rpmbuild/RPMS/${OS}-lzma-sdk457-4.57-2.fc18.noarch.rpm
		"
elif [[ "${OS}" == "linux*" ]]; then

	if [ "${OS}" == "linux64" ]; then
		CHROOT="fedora-17-x86_64"
	else
		CHROOT="fedora-17-i386"
	fi

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
else
	echo "Unsupported OS in ${JOB_NAME}"
	exit 1
fi

function init_chroot() {
	/usr/bin/mock -r ${CHROOT} --clean
	/usr/bin/mock -r ${CHROOT} --init
	/usr/bin/mock -r ${CHROOT} --install ${DEPS}
	/usr/bin/mock -r ${CHROOT} --copyin ~/.ssh "/root/.ssh"
	/usr/bin/mock -r ${CHROOT} --copyin ${WORKSPACE} "/tmp/quake2world"
}

function destroy_chroot() {
	/usr/bin/mock -r ${CHROOT} --clean
}
