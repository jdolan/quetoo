#!/bin/bash -e

#
# Build entry point for GNU / Linux via chroot.
#
CONFIGURE_FLAGS='--with-tests --with-master'

function build() {
	/usr/bin/mock -r ${CHROOT} --cwd /tmp/quake2world --chroot "
		set -e
		autoreconf -i
		scan-build --use-analyzer=/usr/bin/clang ./configure ${CONFIGURE_FLAGS}
		scan-build --use-analyzer=/usr/bin/clang make -C linux ${MAKE_OPTIONS} ${MAKE_TARGETS}
	"

	cp -r /var/lib/mock/${CHROOT}/root/tmp/scan-build* "${WORKSPACE}/${BUILD_TAG}"
	cd ${WORKSPACE}
    tar czf ${BUILD_TAG}.tgz ${BUILD_TAG}
    echo '
    <html><head><title>${BUILD_TAG} results</title></head><frameset rows="100%"><frame src="http://ci.quake2world.net/job/${JOB_NAME}/ws/${BUILD_TAG}/index.html"></frameset></html>' > ${WORKSPACE}/results.html
}

source $(dirname $0)/common.sh

