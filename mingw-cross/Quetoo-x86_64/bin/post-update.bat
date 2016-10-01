
TIMEOUT /t 1

PUSHD %~dp0

FOR %%f IN (*.new) DO MOVE "%%f" "%%nf"

POPD
