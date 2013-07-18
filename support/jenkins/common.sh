# Common functions for all Quake2World / Jenkins build scripts
#
# This file is meant to be included from a platform-specific script. That
# script should implement a "build" function before including this file.
#

# Flags and options passed to ./configure
CONFIGURE_FLAGS="--prefix=/ ${CONFIGURE_FLAGS}"

# Flags and options passed to GNU Make
MAKE_OPTIONS="${MAKE_OPTIONS}"

# Targets invoked to produce the build
MAKE_TARGETS="all ${MAKE_TARGETS}"

#
# Parse common command line arguments and resolve the branch and target. The
# branch and target are specified by the 2nd and 3rd tokens of the Jenkins job
# name. For example, for the job named "quake2world-master-mingw32", the branch
# is "master" and the target is "mingw32".
#
function init() {

	while getopts "c:m:rd" opt; do
		case "${opt}" in
			c)
				CONFIGURE_FLAGS="${opt} ${CONFIGURE_FLAGS}"
				;;
			m)
				MAKE_OPTIONS="${OPTARG} ${MAKE_OPTIONS}"
				;;
			r)
				MAKE_TARGETS="${MAKE_TARGETS} release"
				;;
			d)
				MAKE_TARGETS="${MAKE_TARGETS} dist dist-release"
				;;
			\?)
				echo "Invalid option: -${OPTARG}" >&2
				exit 1
				;;
		esac
	done

	BRANCH=$(echo "${JOB_NAME}" | cut -d\- -f2)
	TARGET=$(echo "${JOB_NAME}" | cut -d\- -f3)

	case "${TARGET}" in
		apple*)
			;;

		linux*)
			if [ "${TARGET}" == "linux64" ]; then
				CHROOT="fedora-17-x86_64"
			else
				CHROOT="fedora-17-i386"
			fi

			CHROOT_PACKAGES="openssh-clients \
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
			;;

		mingw*)
			if [ "${TARGET}" == "mingw64" ]; then
				HOST="x86_64-w64-mingw32"
				ARCH="x86_64"
			else
				HOST="i686-w64-mingw32"
				ARCH="i686"
			fi

			CONFIGURE_FLAGS="--host=${HOST} ${CONFIGURE_FLAGS}"
			MAKE_OPTIONS="HOST=${HOST} ARCH=${ARCH} ${MAKE_OPTIONS}"

			CHROOT="fedora-18-x86_64"

			CHROOT_PACKAGES="openssh-clients \
				rsync \
				${TARGET}-SDL ${TARGET}-SDL_image ${TARGET}-SDL_mixer \
				${TARGET}-curl \
				${TARGET}-physfs \
				${TARGET}-glib2 \
				${TARGET}-libjpeg-turbo libtool \
				${TARGET}-zlib \
				${TARGET}-pkg-config \
				${TARGET}-pdcurses \
				http://maci.satgnu.net/rpmbuild/RPMS/${TARGET}-physfs-2.0.3-3.fc18.noarch.rpm \
				http://maci.satgnu.net/rpmbuild/RPMS/${TARGET}-lzma-sdk457-4.57-2.fc18.noarch.rpm
				"
			;;
		*)
			echo "Unsupported target \"${TARGET}\" in ${JOB_NAME}"
			exit 1
	esac

	echo
	echo "Branch:        ${BRANCH}"
	echo "Target:        ${TARGET}"
	echo "Chroot:        ${CHROOT}"
	echo "Configure:     ${CONFIGURE_FLAGS}"
	echo "Make options:  ${MAKE_OPTIONS}"
	echo "Make targets:  ${MAKE_TARGETS}"
	echo
}

#
# Boot and update the specified chroot.
#
function create_chroot() {
	test "${CHROOT}" && {
		/usr/bin/mock -r ${CHROOT} --clean
		/usr/bin/mock -r ${CHROOT} --init
		/usr/bin/mock -r ${CHROOT} --install ${CHROOT_PACKAGES}
		/usr/bin/mock -r ${CHROOT} --copyin ~/.ssh "/root/.ssh"
		/usr/bin/mock -r ${CHROOT} --copyin ${WORKSPACE} "/tmp/quake2world"
	} || return 0
}

#
# Destroy the current chroot.
#
function destroy_chroot() {
	test "${CHROOT}" && {
		/usr/bin/mock -r ${CHROOT} --clean
	} || return 0
}

#
# Main entry point. Run the build.
#
function main() {
	init $@
	create_chroot
	build
	destroy_chroot
}

main $@
