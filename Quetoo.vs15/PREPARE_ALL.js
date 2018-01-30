if (typeof(log) === 'undefined')
{
	var t_fso = new ActiveXObject("Scripting.FileSystemObject");
	var t_shell = WScript.CreateObject("WScript.Shell");
    var f = t_fso.OpenTextFile("lib.js");
    var s = f.ReadAll();
    f.Close();
    eval(s);
}

include("libs/curses/PREPARE.js");
include("libs/glib/include/PREPARE.js");
include("libs/physfs/PREPARE.js");
include("libs/openal/PREPARE.js");
include("libs/xml/include/PREPARE.js");