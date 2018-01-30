if (!fso.FileExists("tmp.tar.bz2"))
{
	log("Downloading physfs...");
	download("https://icculus.org/physfs/downloads/physfs-2.0.3.tar.bz2", "tmp.tar.bz2");
}

log("Extracting physfs...");
unz("x tmp.tar.bz2 -aos");
unz("e tmp.tar -aos \"physfs-2.0.3\\physfs.h\"");
unz("e tmp.tar -aos \"physfs-2.0.3\\zlib123\\*.h\" -ozlib123\\");
fso.DeleteFile("tmp.tar");