if (!(Test-Path tmp1.zip)) {
    Write-Output "Downloading OpenAL..."
    (New-Object System.Net.WebClient).DownloadFile("http://kcat.strangesoft.net/openal-binaries/openal-soft-1.18.2-bin.zip", "tmp1.zip")
    mkdir AL
    mkdir libs
    mkdir libs/Win32
    mkdir libs/x64
    mkdir bin
    mkdir bin/Win32
    mkdir bin/x64
}

Write-Output "Extracting OpenAL..."
7z e tmp1.zip -aoa "openal-soft-1.18.2-bin/include/AL/*.h" -oAL
7z e tmp1.zip -aoa "openal-soft-1.18.2-bin/libs/Win32/*.lib" -olibs/Win32
7z e tmp1.zip -aoa "openal-soft-1.18.2-bin/libs/Win64/*.lib" -olibs/x64
7z e tmp1.zip -aoa "openal-soft-1.18.2-bin/bin/Win32/soft_oal.dll" -obin/Win32
7z e tmp1.zip -aoa "openal-soft-1.18.2-bin/bin/Win64/soft_oal.dll" -obin/x64
move "bin/Win32/soft_oal.dll" "bin/Win32/OpenAL32.dll"
move "bin/x64/soft_oal.dll" "bin/x64/OpenAL32.dll"