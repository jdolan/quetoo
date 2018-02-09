if (!(Test-Path tmp2.zip)) {
    Write-Output "Downloading OpenAL..."
    (New-Object System.Net.WebClient).DownloadFile("http://kcat.strangesoft.net/openal-binaries/openal-soft-1.18.2-bin.zip", "tmp2.zip")
    mkdir AL
    mkdir libs
    mkdir libs/Win32
    mkdir libs/x64
    mkdir bin
    mkdir bin/Win32
    mkdir bin/x64
}

Write-Output "Extracting OpenAL..."
7z e tmp2.zip -aos "openal-soft-1.18.2-bin/include/AL/*.h" -oAL
7z e tmp2.zip -aos "openal-soft-1.18.2-bin/libs/Win32/*.lib" -olibs/Win32
7z e tmp2.zip -aos "openal-soft-1.18.2-bin/libs/Win64/*.lib" -olibs/x64
7z e tmp2.zip -aos "openal-soft-1.18.2-bin/bin/Win32/*.dll" -obin/Win32
7z e tmp2.zip -aos "openal-soft-1.18.2-bin/bin/Win64/*.dll" -obin/x64
ren "bin/Win32/soft_oal.dll" OpenAL32.dll
ren "bin/x64/soft_oal.dll" OpenAL32.dll