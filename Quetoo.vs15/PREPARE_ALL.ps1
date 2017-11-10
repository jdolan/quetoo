Push-Location "libs\curses\"
[Environment]::CurrentDirectory = $PWD
.\PREPARE.ps1
Pop-Location
[Environment]::CurrentDirectory = $PWD

Push-Location "libs\glib\include\"
[Environment]::CurrentDirectory = $PWD
.\PREPARE.ps1
Pop-Location
[Environment]::CurrentDirectory = $PWD

Push-Location "libs\physfs\"
[Environment]::CurrentDirectory = $PWD
.\PREPARE.ps1
Pop-Location
[Environment]::CurrentDirectory = $PWD

Push-Location "libs\openal\"
[Environment]::CurrentDirectory = $PWD
.\PREPARE.ps1
Pop-Location
[Environment]::CurrentDirectory = $PWD

Push-Location "libs\xml\include\"
[Environment]::CurrentDirectory = $PWD
.\PREPARE.ps1
Pop-Location
[Environment]::CurrentDirectory = $PWD

Push-Location "libs\discord\"
[Environment]::CurrentDirectory = $PWD
.\PREPARE.ps1
Pop-Location
[Environment]::CurrentDirectory = $PWD