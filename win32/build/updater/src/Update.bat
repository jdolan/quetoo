@echo off
echo This program will update the Quake2World data files.
echo Please be patient, this may take a while.

rem This solves the rsync file permissions problem on Vista/Windows 7
set CYGWIN=nontsec
set /p ARCHITECTURE= <arch
rsync.exe -rzhP --delete --exclude=rsync.exe --exclude=cygwin1.dll --exclude=arch --exclude=Update.bat --exclude=Update.exe --exclude=default rsync://jdolan.dyndns.org/quake2world-win32/%ARCHITECTURE%/ .
rem should use --delete here too
rsync.exe -rzhP rsync://jdolan.dyndns.org/quake2world/default/ default
rsync.exe -rzhP --delete rsync://jdolan.dyndns.org/quake2world-win32/%ARCHITECTURE%/default/*.dll default

echo Update complete!
echo Make sure to run this file on a regular basis!
pause
