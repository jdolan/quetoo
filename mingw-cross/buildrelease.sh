pushd /tmp/quake2world-mingw*
MINGW_TARGET=`pwd | cut -d\- -f2`
popd
echo MINGW_TARGET $MINGW_TARGET

if [ "${MINGW_TARGET}" == "mingw64" ]
then
	MINGW_ARCH="x86_64"
elif [ "${MINGW_TARGET}" == "mingw32" ]
then
	MINGW_ARCH="i686"
fi

echo MINGW_ARCH $MINGW_ARCH


function finddll( ){
	if [ "$1" != "" ]; then
		for i in `/usr/bin/${MINGW_ARCH}-w64-mingw32-objdump -p $1 |grep "DLL Name:" |cut -d\: -f2|cut -d\  -f2|sort |uniq`; do
			file=`find /usr/${MINGW_ARCH}-w64-mingw32 2>/dev/null |grep  $i |grep -v .dll.a`
			echo $file
			analyze $file
		done
	fi
}

cp `finddll /tmp/quake2world-${MINGW_TARGET}/bin/quake2world.exe|sort|uniq|grep "\n"` /tmp/quake2world-${MINGW_TARGET}/bin


find /tmp/quake2world-${MINGW_TARGET} -name "*.la" -delete
find /tmp/quake2world-${MINGW_TARGET} -name "*.dll.a" -delete

cp -r bin /tmp/quake2world-${MINGW_TARGET}

rsync -avzP bin lib maci@quake2world.net:/opt/rsync/quake2world-win32/${MINGW_ARCH}
