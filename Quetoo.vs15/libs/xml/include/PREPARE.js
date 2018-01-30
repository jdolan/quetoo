if (!fso.FileExists("tmp.tar.gz"))
{
	log("Downloading libxml2...");
	download("http://xmlsoft.org/sources/libxml2-2.9.4.tar.gz", "tmp.tar.gz");
}

log("Extracting libxml2...");
unz("x tmp.tar.gz -aos");
unz("e tmp.tar -aos \"libxml2-2.9.4\\include\\*.h");
unz("e tmp.tar -aos \"libxml2-2.9.4\\include\\libxml\\*.h\" -olibxml\\");
deleteFile("config.h");
moveFile("win32config.h", "config.h");
deleteFile("tmp.tar");