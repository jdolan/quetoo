if (!(Test-Path tmp.zip)) {
    Write-Output "Downloading curses..."
    (New-Object System.Net.WebClient).DownloadFile("https://www.nano-editor.org/dist/win32-support/pdcurs34.zip", "tmp.zip")
}

Write-Output "Extracting curses..."
7z e tmp.zip -aos "*.h"