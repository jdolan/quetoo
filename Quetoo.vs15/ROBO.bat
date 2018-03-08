@echo off

robocopy "%~1" "%~2" "%~3" /e /w:3 /is /it > nul

echo Copied "%~1/%~3" to %2