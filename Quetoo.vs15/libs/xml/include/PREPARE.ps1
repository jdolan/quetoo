Write-Output "Downloading libxml2..."
(New-Object System.Net.WebClient).DownloadFile("ftp://xmlsoft.org/libxml2/libxml2-2.9.4.tar.gz", "tmp.tar.gz")
Write-Output "Extracting libxml2..."
7z x tmp.tar.gz
7z e tmp.tar -aos "libxml2-2.9.4\include\*.h"
7z e tmp.tar -aos "libxml2-2.9.4\include\libxml\*.h" -olibxml\
Rename-Item win32config.h config.h
Remove-Item "tmp.*"