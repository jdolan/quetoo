if (!(Test-Path tmp.zip)) {
    Write-Output "Downloading OpenAL..."
    (New-Object System.Net.WebClient).DownloadFile("http://kcat.strangesoft.net/openal-binaries/openal-soft-1.17.2-bin.zip", "tmp.zip")
    mkdir AL
    mkdir libs
    mkdir libs/Win32
    mkdir libs/x64
}

Write-Output "Extracting OpenAL..."
7z e tmp.zip -aos "openal-soft-1.17.2-bin/include/AL/*.h" -oAL
7z e tmp.zip -aos "openal-soft-1.17.2-bin/libs/Win32/*.lib" -olibs/Win32
7z e tmp.zip -aos "openal-soft-1.17.2-bin/libs/Win64/*.lib" -olibs/x64