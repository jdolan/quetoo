MINGW_ARCH=`find /tmp -name quake2world-mingw* 2>/dev/null|cut -d\- -f2`

TARGET="i686"
if [ ${MINGW_ARCH} == "mingw64" ]
then
	TARGET="x86_64"
fi

function finddll( ){
	if [ "$1" != "" ]; then
		for i in `/usr/bin/${TARGET}-w64-mingw32-objdump -p $1 |grep "DLL Name:" |cut -d\: -f2|cut -d\  -f2|sort |uniq`; do
			file=`find /usr/${TARGET}-w64-mingw32 2>/dev/null |grep  $i |grep -v .dll.a`
			echo $file
			analyze $file
		done
	fi
}

cp `finddll /tmp/quake2world-${MINGW_ARCH}/bin/quake2world.exe|sort|uniq|grep "\n"` /tmp/quake2world-${MINGW_ARCH}/bin
cp `finddll /tmp/quake2world-${MINGW_ARCH}/bin/q2wmap.exe|sort|uniq|grep "\n"` /tmp/quake2world-${MINGW_ARCH}/bin


find /tmp/quake2world-${MINGW_ARCH} -name "*.la" -delete
find /tmp/quake2world-${MINGW_ARCH} -name "*.dll.a" -delete

cp -r bin /tmp/quake2world-${MINGW_ARCH}

#rsync -avzP bin lib maci@quake2world.net:/opt/rsync/quake2world-win32/${TARGET}
