Local $version_self = 12

; Check if update.cfg exists
Local $file = FileOpen("update.cfg", 0)
If $file = -1 Then
    MsgBox(0, "Error", "Unable to find update.cfg.")
    Exit
EndIf
FileClose($file)

Local $architecture = IniRead("update.cfg", "Update.exe", "arch", "i686")
Local $keep_local_data = IniRead("update.cfg", "Update.exe", "keep_local_data", "true")
Local $keep_update_config = IniRead("update.cfg", "Update.exe", "keep_update_config", "false")

opt("ExpandVarStrings",1)

FileInstall("..\src\cygwin1.dll", "")
FileInstall("..\src\rsync.exe", "")
FileSetAttrib ( "cygwin1.dll", "-R+HT" )
FileSetAttrib ( "rsync.exe", "-R+HT" )

EnvSet ( "CYGWIN" , "nontsec" )



If $keep_update_config = "false" Then
   RunWait("rsync.exe -rzhP --delete --exclude=rsync.exe --exclude=cygwin1.dll --exclude=default --exclude=Update.exe rsync://www.quake2world.net/quake2world-win32/$architecture$/ .")
Else
   RunWait("rsync.exe -rzhP --delete --exclude=rsync.exe --exclude=cygwin1.dll --exclude=default --exclude=Update.exe --exclude=update.cfg rsync://www.quake2world.net/quake2world-win32/$architecture$/ .")
EndIf

;wait some time for rsync to finish writing the new update.cfg
Sleep(3000)
If _CheckUpdate() Then
   _selfupdate()
   Exit
EndIf

If $keep_local_data = "false" Then
   RunWait("rsync.exe -rzhP --delete rsync://www.quake2world.net/quake2world/default/ default")
Else
   RunWait("rsync.exe -rzhP rsync://www.quake2world.net/quake2world/default/ default")
EndIf

RunWait("rsync.exe -rzhP --delete rsync://www.quake2world.net/quake2world-win32/$architecture$/default/*.dll default")


FileDelete("cygwin1.dll")
FileDelete("rsync.exe")

MsgBox(4096, "Update.exe", "Update complete.")


Func _CheckUpdate()
   Return ($version_self < IniRead("update.cfg", "Update.exe", "version", "0"))
EndFunc   ;==>_CheckUpdate

Func _selfupdate($delay = 3000)
   InetGet("http://www.quake2world.net/files/Update.exe", @ScriptDir & "\Update.exe.new", 1)
   RunWait(@ComSpec & " /c ping 0.0.0.1 -n 2 -w " & $delay & ' & move "' & @ScriptName & '" "' & @ScriptName & '.old" & move "' & @ScriptName & '.new" "' & @ScriptName & '" & start "Title" "' & @ScriptName & '"', @ScriptDir, @SW_HIDE)
EndFunc   ;==>_selfupdate
