del bin /S /Q

mkdir bin
mkdir bin\Win32Debug\
mkdir bin\Win32Release\
mkdir bin\Win64Debug\
mkdir bin\Win64Release\

cd sdl_mixer\
call prepare.bat
cd ..\gettext\
call prepare.bat
cd ..\glib\
call prepare.bat
