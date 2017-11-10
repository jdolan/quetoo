if (!(Test-Path tmp.zip)) {
    Write-Output "Downloading Discord Rich Presence..."
    (New-Object System.Net.WebClient).DownloadFile("https://github.com/discordapp/discord-rpc/releases/download/v2.0.1/discord-rpc-2.0.1.zip", "tmp.zip")
}

Write-Output "Extracting Discord Rich Presence..."
7z x tmp.zip -aos -r *.dll *.lib *.h
move discord-rpc-2.0.1/win32-static/ win32
move discord-rpc-2.0.1/win64-static/ x64
Remove-Item -recurse -force discord-rpc-2.0.1
Start-Sleep -s 10