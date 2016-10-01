Write-Output "Downloading glib..."
(New-Object System.Net.WebClient).DownloadFile("http://ftp.gnome.org/pub/gnome/sources/glib/2.26/glib-2.26.1.tar.bz2", "tmp.tar.bz2")
Write-Output "Extracting glib..."
7z x tmp.tar.bz2
7z e tmp.tar -aos "glib-2.26.1\glib\*.h" -oglib\
7z e tmp.tar -aos "glib-2.26.1\gobject\*.h" -ogobject\
7z e tmp.tar -aos "glib-2.26.1\gio\*.h" -ogio\
7z e tmp.tar -aos "glib-2.26.1\glib\glibconfig.h.win32" -oglib\
Move-Item glib\glib.h glib.h
Move-Item glib\glibconfig.h.win32 glibconfig.h
Remove-Item "tmp.*"