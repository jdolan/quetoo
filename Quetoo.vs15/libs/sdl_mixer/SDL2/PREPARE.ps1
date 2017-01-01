If (!(Test-Path tmp.zip)) {
    Write-Output "Downloading SDL_mixer..."
    (New-Object System.Net.WebClient).DownloadFile("https://www.libsdl.org/projects/SDL_mixer/release/SDL2_mixer-devel-2.0.1-VC.zip", "tmp.zip")
}

Write-Output "Extracting SDL_mixer..."
7z e tmp.zip -aos "SDL2_mixer-2.0.1\include\*.h"
7z e tmp.zip -aos "SDL2_mixer-2.0.1\lib\x86\*" -o"..\lib\Win32\"
7z e tmp.zip -aos "SDL2_mixer-2.0.1\lib\x64\*" -o"..\lib\x64\"