FileInstall("..\src\Update.bat", "")
FileInstall("..\src\cygwin1.dll", "")
FileInstall("..\src\rsync.exe", "")
FileInstall("arch", "arch")


RunWait("Update.bat")



FileSetAttrib ( "Update.bat", "-RH" )
FileSetAttrib ( "cygwin1.dll", "-RH" )
FileSetAttrib ( "rsync.exe", "-RH" )
FileSetAttrib ( "arch", "-RH" )

FileDelete("Update.bat")
FileDelete("cygwin1.dll")
FileDelete("rsync.exe")
FileDelete("arch")




