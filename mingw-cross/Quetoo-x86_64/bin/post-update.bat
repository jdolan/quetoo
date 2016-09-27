@ECHO OFF

TIMEOUT /t 1

SET BIN=%~dp0

FOR %%f IN (BIN/*.new) DO RENAME "%%f" "%%~nf"
