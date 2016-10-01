Write-Output "Downloading curses..."
(New-Object System.Net.WebClient).DownloadFile("http://downloads.sourceforge.net/project/pdcurses/pdcurses/3.4/pdcurs34.zip?r=https%3A%2F%2Fsourceforge.net%2Fprojects%2Fpdcurses%2Ffiles%2Fpdcurses%2F&ts=1475253175&use_mirror=heanet", "tmp.zip")
Write-Output "Extracting curses..."
7z e tmp.zip -aos "*.h"
Remove-Item "tmp.*"