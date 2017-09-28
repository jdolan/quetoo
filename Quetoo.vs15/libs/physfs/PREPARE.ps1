If (!(Test-Path tmp3.tar.bz2)) {
    Write-Output "Downloading physfs..."
    (New-Object System.Net.WebClient).DownloadFile("https://icculus.org/physfs/downloads/physfs-3.0.0.tar.bz2", "tmp3.tar.bz2")
}

Write-Output "Extracting physfs 3.0.0..."
7z x tmp3.tar.bz2
7z e tmp3.tar -aos "physfs-3.0.0\src\physfs.h" -o".\"
Remove-Item "tmp3.tar"