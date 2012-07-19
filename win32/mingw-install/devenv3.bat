@echo off
Set SCRIPTDIR=%CD%
Set TARGETDIR="C:\q2wdevenv"
Set MINGW_GET_URL="http://downloads.sourceforge.net/project/mingw/Installer/mingw-get/mingw-get-0.5-beta-20120426-1/mingw-get-0.5-mingw32-beta-20120426-1-bin.zip"
Set MINGW_I686_URL="http://downloads.sourceforge.net/project/mingw-w64/Toolchains targetting Win32/Personal Builds/rubenvb/release/i686-w64-mingw32-gcc-4.7.1-release-win32_rubenvb.7z"
Set MINGW_X86_64_URL="http://downloads.sourceforge.net/project/mingw-w64/Toolchains targetting Win64/Personal Builds/rubenvb/release/x86_64-w64-mingw32-gcc-4.7.1-release-win64_rubenvb.7z"

mkdir %TARGETDIR%
cscript wget7z.vbs %TARGETDIR%
cd %TARGETDIR%

IF NOT EXIST wget.exe GOTO DLERR
IF NOT EXIST 7za.exe GOTO DLERR

wget -c %MINGW_GET_URL%
7za x *.zip
del *.zip

cd bin
mingw-get install mingw-get
mingw-get install mingw-developer-toolkit msys-zip msys-unzip msys-wget
mingw-get remove --recursive libltdl libintl libiconv libgettextpo libexpat libpthreadgc libgomp gettext libtool libstdc++ libgcc
mingw-get remove --recursive libltdl libintl libiconv libgettextpo libexpat libpthreadgc libgomp gettext libtool libstdc++ libgcc
mingw-get remove --recursive libltdl libintl libiconv libgettextpo libexpat libpthreadgc libgomp gettext libtool libstdc++ libgcc
mingw-get remove --recursive mingw-get

cd ..

rem #sleep 5 secs
ping -n 5 127.0.0.1 > NUL

rmdir /S /Q var lib libexec include share\gettext share\locale share\doc
del bin\mingw-get.exe

xcopy bin\* msys\1.0\bin /E
xcopy share\* msys\1.0\share /E
rmdir /S /Q bin
rmdir /S /Q share

mkdir msys\1.0\home\%UserName%\mingw-install\scripts
echo %SCRIPTDIR%\mingw-install\scripts\
xcopy %SCRIPTDIR%\scripts\* msys\1.0\home\%UserName%\mingw-install\scripts /E
xcopy %SCRIPTDIR%\install.sh msys\1.0\home\%UserName%\mingw-install /E
xcopy %SCRIPTDIR%\..\switch_arch.sh msys\1.0\home\%UserName% /E

rem msys\1.0\bin\sh.exe -c "echo export CPPFLAGS=\'-I/mingw/include -I/mingw/local/include\' >> /etc/profile"
rem msys\1.0\bin\sh.exe -c "echo export LDFLAGS=\'-L/mingw/lib -L/mingw/local/lib\' >> /etc/profile"
rem msys\1.0\bin\sh.exe -c "echo export ACLOCAL=\'aclocal -I /mingw/local/share/aclocal\' >> /etc/profile"
rem msys\1.0\bin\sh.exe -c "echo export PKG_CONFIG_PATH=/mingw/local/lib/pkgconfig >> /etc/profile"
msys\1.0\bin\sh.exe -c "echo export PATH=/mingw/local/bin:'$PATH' >> /etc/profile"

msys\1.0\bin\sh.exe -c "echo "%TARGETDIR%"\\\mingw32 /mingw > /etc/fstab"


rem #Downloading and installing gcc
wget -c %MINGW_I686_URL% %MINGW_X86_64_URL%
7za x *.7z

rem #Cleanup
del *.exe *7z
pause
msys\1.0\msys.bat

:DLERR
echo Error downloading files
pause
