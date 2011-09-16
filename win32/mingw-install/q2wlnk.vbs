set WshShell = WScript.CreateObject("WScript.Shell" ) 
strDesktop = WshShell.SpecialFolders("Desktop" ) 
set oShellLink = WshShell.CreateShortcut(strDesktop & "\Q2Wdevenv.lnk" ) 
oShellLink.TargetPath = "C:\q2wdevenv\msys\1.0\msys.bat" 
oShellLink.WindowStyle = 1 
oShellLink.IconLocation = "C:\q2wdevenv\msys\1.0\msys.ico" 
oShellLink.Description = "Q2Wdevenv" 
oShellLink.WorkingDirectory = "C:\q2wdevenv\msys\1.0" 
oShellLink.Save 