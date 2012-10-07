PKGNAME="ntldd"
PKGVER="master"

SOURCE=https://github.com/LRN/${PKGNAME}/tarball/${PKGVER}

pushd ../source
wget --no-check-certificate -c -O ${PKGNAME}-${PKGVER} $SOURCE 
popd 

tar xzf ../source/${PKGNAME}-${PKGVER}.tar.gz

cd *${PKGNAME}*

sh makeldd.sh 
cp ntldd.exe /mingw/local/bin/ldd.exe
