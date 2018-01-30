if (!fso.FileExists("tmp.tar.xz"))
{
	log("Downloading glib...");
	download("http://ftp.gnome.org/pub/GNOME/sources/glib/2.50/glib-2.50.1.tar.xz", "tmp.tar.xz");
}

log("Extracting glib...");
unz("x tmp.tar.xz -aos");
unz("e tmp.tar -aos \"glib-2.50.1\\glib\\*.h\" -oglib\\");
unz("e tmp.tar -aos \"glib-2.50.1\\glib\\deprecated\\*.h\" -oglib\deprecated\\");
unz("e tmp.tar -aos \"glib-2.50.1\\gobject\\*.h\" -ogobject\\");
unz("e tmp.tar -aos \"glib-2.50.1\\gio\\*.h\" -ogio\\");
fso.DeleteFile("glib.h");
fso.MoveFile("glib/glib.h", "glib.h");
fso.DeleteFile("tmp.tar");