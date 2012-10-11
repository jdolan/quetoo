#!/bin/bash

#exit on error
set -e
set -o errexit

PKGNAME="python"
PKGVER="2.7.3"
SOURCE=http://www.python.org/ftp/${PKGNAME}/${PKGVER}/${PKGNAME}-${PKGVER}.msi

pushd ../source
wget -c ${SOURCE}
msiexec //i ${PKGNAME}-${PKGVER}.msi //qn ALLUSERS=1 TARGETDIR=C:\\q2wdevenv\\Python27
popd 

echo -e '#!/bin/sh\nC:/q2wdevenv/Python27/python.exe $*' > /bin/python
cp /bin/python /bin/python2
cp /bin/python /bin/python2.5
cp /bin/python /bin/python2.7
#echo 'export PATH=$PATH:/c/Python27/Tools/Scripts' >> /etc/profile

