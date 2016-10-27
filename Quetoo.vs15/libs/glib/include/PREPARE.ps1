If (!(Test-Path tmp.tar.xz)) {
    Write-Output "Downloading glib..."
    (New-Object System.Net.WebClient).DownloadFile("http://ftp.gnome.org/pub/GNOME/sources/glib/2.50/glib-2.50.1.tar.xz", "tmp.tar.xz")
}

Write-Output "Extracting glib..."
7z x tmp.tar.xz
7z e tmp.tar -aos "glib-2.50.1\glib\*.h" -oglib\
7z e tmp.tar -aos "glib-2.50.1\glib\deprecated\*.h" -oglib\deprecated\
7z e tmp.tar -aos "glib-2.50.1\gobject\*.h" -ogobject\
7z e tmp.tar -aos "glib-2.50.1\gio\*.h" -ogio\
Move-Item glib\glib.h glib.h
Remove-Item "tmp.tar"