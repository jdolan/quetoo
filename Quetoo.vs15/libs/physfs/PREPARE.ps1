If (!(Test-Path tmp4.tar.bz2)) {
    Write-Output "Downloading physfs..."
    (New-Object System.Net.WebClient).DownloadFile("https://icculus.org/physfs/downloads/physfs-3.0.1.tar.bz2", "tmp4.tar.bz2")
}

Write-Output "Extracting physfs 3.0.1..."
7z x tmp4.tar.bz2
7z e tmp4.tar -aos "physfs-3.0.1\src\physfs.h" -o".\"
Remove-Item "tmp4.tar"