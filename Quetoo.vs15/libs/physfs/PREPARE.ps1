Write-Output "Downloading physfs..."
(New-Object System.Net.WebClient).DownloadFile("https://icculus.org/physfs/downloads/physfs-2.0.3.tar.bz2", "tmp.tar.bz2")
Write-Output "Extracting physfs..."
7z x tmp.tar.bz2
7z e tmp.tar -aos "physfs-2.0.3\physfs.h"
7z e tmp.tar -aos "physfs-2.0.3\zlib123\*.h" -ozlib123\
Remove-Item "tmp.*"