@echo off
echo This program will update Quake2World.
echo Please be patient, this may take a while.
rsync.exe -rz --progress --exclude=rsync.exe --exclude=cygwin1.dll rsync://satgnu.net/quake2world-win32 .
rsync.exe --delete -rz --progress rsync://satgnu.net/quake2world temp
xcopy temp\default default /E /Y >nul
echo Update complete!
echo If this was the first time you run Update.bat the next update should be a lot faster. ;)
echo Make sure to run this file on a regular basis!
pause
