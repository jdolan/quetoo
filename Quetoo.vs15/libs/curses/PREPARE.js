if (!fso.FileExists("tmp.zip"))
{
	log("Downloading curses...");
	download("https://www.nano-editor.org/dist/win32-support/pdcurs34.zip", "tmp.zip");
}

log("Extracting curses...");
unz("e tmp.zip -aos \"*.h\"");