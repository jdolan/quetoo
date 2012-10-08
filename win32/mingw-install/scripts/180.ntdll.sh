PKGNAME="LRN-ntldd"
PKGVER="master"

SOURCE=https://github.com/LRN/ntldd/tarball/${PKGVER}

pushd ../source
wget --no-check-certificate -c -O ${PKGNAME}-${PKGVER}.tar.gz $SOURCE 
popd 

tar xzf ../source/${PKGNAME}-${PKGVER}.tar.gz

cd ${PKGNAME}*

sh makeldd.sh 
cp ntldd.exe /mingw/local/bin/ldd.exe
