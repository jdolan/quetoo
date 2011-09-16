SOURCE=http://www.antisphere.com/Tools/AntTweakBar/AntTweakBar
wget -c $SOURCE
unzip AntTweakBar*
cd AntTweakBar*
cp include/* /mingw/include
cp lib/AntTweakBar.lib /mingw/lib
cp lib/AntTweakBar.dll /mingw/bin