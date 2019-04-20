IF [%1] == [] GOTO INVALID
IF [%2] == [] GOTO INVALID
IF [%1] == [""] GOTO INVALID
IF [%2] == [""] GOTO INVALID
if "%QUETOO_HOME%" == "" GOTO INVALID

set quetoo_folder=%~1
set build_platform=%~2
set build_configuration=%~3

call ROBO "../mingw-cross/Quetoo-i686/etc/fonts/" "%quetoo_folder%/etc/fonts/" *
call ROBO "../src/client/renderer/shaders/" "%quetoo_folder%/lib/default/shaders/" *
call ROBO "../src/cgame/default/ui/" "%quetoo_folder%/lib/default/ui/" *.css
call ROBO "../src/client/ui/" "%quetoo_folder%/lib/default/ui/" *.css
call ROBO "../src/cgame/default/ui/" "%quetoo_folder%/lib/default/ui/" *.json
call ROBO "../src/client/ui/" "%quetoo_folder%/lib/default/ui/" *.json

call ROBO "../../Objectively/Objectively.vs15/bin/%build_platform%%build_configuration%/" "%quetoo_folder%/bin/" Objectively.*
call ROBO "../../Objectively/Objectively.vs15/libs/dlfcn/%build_platform%/" "%quetoo_folder%/bin/" *.dll
call ROBO "../../ObjectivelyMVC/ObjectivelyMVC.vs15/bin/%build_platform%%build_configuration%/" "%quetoo_folder%/bin/" ObjectivelyMVC.*

call ROBO "libs/gettext/%build_platform%/bin/" "%quetoo_folder%/bin/" *.dll
call ROBO "libs/glib/%build_platform%/bin/" "%quetoo_folder%/bin/" *.dll

call ROBO "../../ObjectivelyMVC/ObjectivelyMVC.vs15/libs/sdl/lib/%build_platform%/" "%quetoo_folder%/bin/" *.dll
call ROBO "../../ObjectivelyMVC/ObjectivelyMVC.vs15/libs/sdl_image/lib/%build_platform%/" "%quetoo_folder%/bin/" *.dll
call ROBO "../../ObjectivelyMVC/ObjectivelyMVC.vs15/libs/sdl_ttf/lib/%build_platform%/" "%quetoo_folder%/bin/" *.dll

call ROBO "libs/openal/bin/%build_platform%/" "%quetoo_folder%/bin/" *.dll

GOTO DONE

:INVALID
echo "No build input folder, or QUETOO_HOME not defined. Gonedy."

:DONE