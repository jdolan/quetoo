$platform = If ($env:PLATFORM = "Win32") {"Win32"} Else {"x64"}

# make the folders
rmdir Quetoo -Recurse -ErrorAction SilentlyContinue
mkdir Quetoo -ErrorAction SilentlyContinue
mkdir Quetoo\bin -ErrorAction SilentlyContinue
mkdir Quetoo\etc -ErrorAction SilentlyContinue
mkdir Quetoo\etc\fonts -ErrorAction SilentlyContinue
mkdir Quetoo\lib -ErrorAction SilentlyContinue
mkdir Quetoo\lib\default -ErrorAction SilentlyContinue
mkdir Quetoo\lib\default\shaders -ErrorAction SilentlyContinue
mkdir Quetoo\share -ErrorAction SilentlyContinue
mkdir Quetoo\share\default -ErrorAction SilentlyContinue

# move stuff that we need from old crap
copy "..\mingw-cross\Quetoo-i686\etc\fonts\*" "Quetoo\etc\fonts\"
copy "..\src\client\renderer\shaders\*" "Quetoo\lib\default\shaders\"
copy "..\mingw-cross\Quetoo-i686\bin\*" "Quetoo\bin\"

# move our build outputs
$outdir = "bin\${platform}Release\"

# start with game/cgame
copy "$outdir\cgame.dll" "Quetoo\lib\default\"
copy "$outdir\cgame.pdb" "Quetoo\lib\default\"

copy "$outdir\game.dll" "Quetoo\lib\default\"
copy "$outdir\game.pdb" "Quetoo\lib\default\"

# now do the quetoo-* programs
copy "$outdir\que*.exe" "Quetoo\bin\"
copy "$outdir\que*.pdb" "Quetoo\bin\"

# copy external libs
$libdir = "libs"
copy "$libdir\sdl_mixer\lib\$platform\*.dll" "Quetoo\bin\"
copy "$libdir\glib\$platform\bin\*.dll" "Quetoo\bin\"
copy "$libdir\gettext\$platform\bin\*.dll" "Quetoo\bin\"

$libdir = "..\..\ObjectivelyMVC\ObjectivelyMVC.vs15\libs"
copy "$libdir\sdl\lib\$platform\*.dll" "Quetoo\bin\"
copy "$libdir\sdl_image\lib\$platform\*.dll" "Quetoo\bin\"
copy "$libdir\sdl_ttf\lib\$platform\*.dll" "Quetoo\bin\"