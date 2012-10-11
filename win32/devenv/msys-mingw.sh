#!/bin/sh

#exit on error
set -e
set -o errexit

SOURCEDIR=`pwd`
TARGETDIR="C:/q2wdevenv"

SEVENZIP_URL="http://mirror.transact.net.au/sourceforge/s/project/se/sevenzip/7-Zip/9.20/7za920.zip"
MINGW_GET_URL="http://downloads.sourceforge.net/project/mingw/Installer/mingw-get/mingw-get-0.5-beta-20120426-1/mingw-get-0.5-mingw32-beta-20120426-1-bin.zip"
MINGW_I686_URL="http://sourceforge.net/projects/mingw-w64/files/Toolchains targetting Win32/Personal Builds/rubenvb/gcc-4.7-release/i686-w64-mingw32-gcc-4.7.2-release-win32_rubenvb.7z"
MINGW_X86_64_URL="http://sourceforge.net/projects/mingw-w64/files/Toolchains targetting Win64/Personal Builds/rubenvb/gcc-4.7-release/x86_64-w64-mingw32-gcc-4.7.2-release-win64_rubenvb.7z"

[ -d ${TARGETDIR} ] || mkdir ${TARGETDIR}

cd ${TARGETDIR}

wget -c ${SEVENZIP_URL}
unzip 7za*.zip 7za.exe
rm 7za*.zip

wget ${MINGW_GET_URL}
unzip mingw-get*.zip
rm mingw-get*.zip

cd bin
./mingw-get.exe install mingw-developer-toolkit msys-zip msys-unzip msys-wget
./mingw-get.exe remove --recursive libltdl libintl libiconv libgettextpo libexpat libpthreadgc libgomp gettext libtool libstdc++ libgcc
./mingw-get.exe remove --recursive libltdl libintl libiconv libgettextpo libexpat libpthreadgc libgomp gettext libtool libstdc++ libgcc
./mingw-get.exe remove --recursive libltdl libintl libiconv libgettextpo libexpat libpthreadgc libgomp gettext libtool libstdc++ libgcc

cd ..

sleep 5

rm -Rf var lib include libexec share/doc share/gettext share/locale bin/mingw-get.exe



mkdir autotools
mv bin autotools 
mv share autotools
./msys/1.0/bin/rsync.exe -r autotools/ mingw32
./msys/1.0/bin/rsync.exe -r autotools/ mingw64


mv ${TARGETDIR}/msys/1.0/* ${TARGETDIR}/msys
rmdir ${TARGETDIR}/msys/1.0

mkdir -p ${TARGETDIR}/msys/home/${USERNAME}/devenv/scripts

cp -r	${SOURCEDIR}/scripts/* 			${TARGETDIR}/msys/home/${USERNAME}/devenv/scripts
cp		${SOURCEDIR}/install.sh 		${TARGETDIR}/msys/home/${USERNAME}/devenv
cp		${SOURCEDIR}/../switch_arch.sh 	${TARGETDIR}/msys/home/${USERNAME}

cp ${SOURCEDIR}/*.txt ${TARGETDIR}


echo "export LDFLAGS='-L/mingw/lib -L/mingw/local/lib'" >> ${TARGETDIR}/msys/etc/profile
echo "export ACLOCAL='aclocal -I /mingw/local/share/aclocal'" >> ${TARGETDIR}/msys/etc/profile
echo "export PKG_CONFIG_PATH='/mingw/local/lib/pkgconfig'" >> ${TARGETDIR}/msys/etc/profile
echo "export PATH=/mingw/local/bin:\${PATH}" >> ${TARGETDIR}/msys/etc/profile
echo "${TARGETDIR}/mingw32 /mingw" > ${TARGETDIR}/msys/etc/fstab


wget -c $MINGW_I686_URL $MINGW_X86_64_URL

7za.exe x *.7z

#Cleanup
rm *.exe *7z
echo done

cmd \/C msys\\msys.bat
exit 0


