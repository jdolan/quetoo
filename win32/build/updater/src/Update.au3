Local $version_self = 2

Local $file = FileOpen("update.cfg", 0)

; Check if file opened for reading OK
If $file = -1 Then
    MsgBox(0, "Error", "Unable to find update.cfg.")
    Exit
EndIf
FileClose($file)

Local $version_current = IniRead("update.cfg", "Update.exe", "version", "0")
Local $architecture = IniRead("update.cfg", "Update.exe", "arch", "i686")
Local $delete_local_data = IniRead("update.cfg", "Update.exe", "delete_local_data", "false")


If $version_self < $version_current Then
  ;download new version
  InetGet("http://satgnu.net/files/quake2world/" & $architecture & "/Update.exe", @ScriptDir & "\Update.exe.new")
  FileMove(@ScriptFullPath, @ScriptDir & "\OLDUpdate.exe")
  FileMove(@ScriptDir & "\Update.exe.new", @ScriptFullPath)
  Run(@ScriptName)
  Exit
EndIf


opt("ExpandVarStrings",1)

FileInstall("..\src\cygwin1.dll", "")
FileInstall("..\src\rsync.exe", "")

EnvSet ( "CYGWIN" , "nontsec" )
EnvSet ( "CYGWIN" , "nontsec" )


RunWait("rsync.exe -rzhP --delete --exclude=rsync.exe --exclude=cygwin1.dll --exclude=default --exclude=Update.exe rsync://jdolan.dyndns.org/quake2world-win32/$architecture$/ .")
If $delete_local_data = "true" Then
   RunWait("rsync.exe -rzhP --delete rsync://jdolan.dyndns.org/quake2world/default/ default")
Else
   RunWait("rsync.exe -rzhP rsync://jdolan.dyndns.org/quake2world/default/ default")
EndIf

RunWait("rsync.exe -rzhP --delete rsync://jdolan.dyndns.org/quake2world-win32/$architecture$/default/*.dll default")


FileSetAttrib ( "cygwin1.dll", "-RH" )
FileSetAttrib ( "rsync.exe", "-RH" )

FileDelete("cygwin1.dll")
FileDelete("rsync.exe")

MsgBox(4096, "Update.exe", "Update complete.")