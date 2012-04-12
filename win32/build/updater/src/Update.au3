FileInstall("..\src\Update.bat", "")
FileInstall("..\src\cygwin1.dll", "")
FileInstall("..\src\rsync.exe", "")

RunWait("Update.bat")

FileSetAttrib ( "Update.bat", "-RH" )
FileSetAttrib ( "cygwin1.dll", "-RH" )
FileSetAttrib ( "rsync.exe", "-RH" )

FileDelete("Update.bat")
FileDelete("cygwin1.dll")
FileDelete("rsync.exe")




