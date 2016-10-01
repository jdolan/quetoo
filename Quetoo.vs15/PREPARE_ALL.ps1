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

Push-Location "libs\sdl_mixer\SDL2\"
[Environment]::CurrentDirectory = $PWD
.\PREPARE.ps1
Pop-Location
[Environment]::CurrentDirectory = $PWD

Push-Location "libs\xml\include\"
[Environment]::CurrentDirectory = $PWD
.\PREPARE.ps1
Pop-Location
[Environment]::CurrentDirectory = $PWD