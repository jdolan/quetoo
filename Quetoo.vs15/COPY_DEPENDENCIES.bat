IF [%1] == [] GOTO INVALID
IF [%2] == [] GOTO INVALID
IF [%1] == [""] GOTO INVALID
IF [%2] == [""] GOTO INVALID

set quetoo_folder=%1
set build_platform=%2
set build_configuration=%3

copy "..\..\Objectively\Objectively.vs15\bin\%build_platform%%build_configuration%\Objectively*" "%quetoo_folder%\bin\*" /y
copy "..\..\ObjectivelyMVC\ObjectivelyMVC.vs15\bin\%build_platform%%build_configuration%\ObjectivelyMVC*" "%quetoo_folder%\bin\*" /y

copy "libs\gettext\%build_platform%\bin\*.dll" "%quetoo_folder%\bin\*" /y
copy "libs\glib\%build_platform%\bin\*.dll" "%quetoo_folder%\bin\*" /y
copy "libs\sdl_mixer\lib\%build_platform%\*.dll" "%quetoo_folder%\bin\*" /y

copy "..\..\ObjectivelyMVC\ObjectivelyMVC.vs15\libs\sdl\lib\%build_platform%\*.dll" "%quetoo_folder%\bin\*" /y
copy "..\..\ObjectivelyMVC\ObjectivelyMVC.vs15\libs\sdl_image\lib\%build_platform%\*.dll" "%quetoo_folder%\bin\*" /y
copy "..\..\ObjectivelyMVC\ObjectivelyMVC.vs15\libs\sdl_ttf\lib\%build_platform%\*.dll" "%quetoo_folder%\bin\*" /y

GOTO DONE

:INVALID
echo "No build input folder. Gonedy."

:DONE