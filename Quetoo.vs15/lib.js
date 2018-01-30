function log(msg)
{
	WScript.Echo(msg);
}

var shell = WScript.CreateObject("WScript.Shell");
var debug = true;
var fso = WScript.CreateObject("Scripting.FileSystemObject");

function include(file)
{
	if (!fso.FileExists(file))
	{
		log("Can't include: " + file);
		return;
	}

	var f = fso.OpenTextFile(file);
	var s = f.ReadAll();
	f.Close();

	var oldDir = shell.CurrentDirectory;
	shell.CurrentDirectory = fso.GetParentFolderName(file);

	eval(s);

	shell.CurrentDirectory = oldDir;
}

function download(url, output)
{
	if (debug)
		log("DOWNLOAD DEBUG: " + url + " " + output);
	
	var xhr = WScript.CreateObject("MSXML2.ServerXMLHTTP");
	xhr.open("GET", url, false);
	xhr.send();

	if (xhr.status !== 200)
		throw new Error("couldn't download!");

	if (fso.FileExists(output))
		fso.DeleteFile(output);
	
	var stream = WScript.CreateObject("ADODB.Stream");
	stream.Type = 1;
	stream.Open();
	stream.Write(xhr.responseBody);
	stream.SaveToFile(output, 2);
	stream.Close();
}

function robo(input, output, filter)
{
	if (debug)
		log("ROBO DEBUG: " + input + " " + output + " " + filter);
	
	var arguments = '"' + input + '" "' + output + '" "' + filter + '" /e /w:3';
	var exec = shell.Exec('robocopy ' + arguments);
	
	while (exec.status === 0)
		WScript.Sleep(100);
	
	if (exec.status === 1)
	{
		if (debug)
			log(exec.StdOut.ReadAll());
		else
			log('Copied \"' + input + filter + '" to ' + output);
	}
	else
		log(exec.StdErr.ReadAll());	
}

function unz(args)
{
	if (debug)
		log("UNZ DEBUG: " + args);
	
	var exec = shell.Exec('7z ' + args);
	
	while (exec.status === 0)
		WScript.Sleep(100);
	
	if (exec.status === 1)
		if (debug)
			log(exec.StdOut.ReadAll());
	else
		log(exec.StdErr.ReadAll());	
}

function deleteFile(file)
{
	if (debug)
		log("DELETE: " + file);
	
	if (!fso.FileExists(file))
	{
		if (debug)
			log("Doesn't exist.");
	}
	else
	{
		fso.DeleteFile(file);
		
		if (debug)
			log("Deleted.");
	}
}

function moveFile(oldPath, newPath)
{
	if (debug)
		log("MOVE " + oldPath + " -> " + newPath);
	
	if (!fso.FileExists(oldPath))
		throw new Error("can't move file, not there: " + oldPath);
	
	fso.MoveFile(oldPath, newPath);
}