IF [%1] == [] GOTO INVALID
IF [%2] == [] GOTO INVALID
IF [%1] == [""] GOTO INVALID
IF [%2] == [""] GOTO INVALID

set quetoo_folder=%1
set build_platform=%2
set build_configuration=%3

robocopy "../mingw-cross/Quetoo-i686/etc/fonts/" "%quetoo_folder%/etc/fonts/" * /E /NJH /NJS /FP /NP /V | findstr /v "*EXTRA File"
robocopy "../mingw-cross/Quetoo-i686/bin/" "%quetoo_folder%/bin/" * /E /NJH /NJS /FP /NP /V | findstr /v "*EXTRA File"

robocopy "../../Objectively/Objectively.vs15/bin/%build_platform%%build_configuration%/" "%quetoo_folder%/bin/" Objectively.* /E /NJH /NJS /FP /NP /V | findstr /v "*EXTRA File"
robocopy "../../ObjectivelyMVC/ObjectivelyMVC.vs15/bin/%build_platform%%build_configuration%/" "%quetoo_folder%/bin/" ObjectivelyMVC.* /E /NJH /NJS /FP /NP /V | findstr /v "*EXTRA File"

robocopy "libs/gettext/%build_platform%/bin/" "%quetoo_folder%/bin/" *.dll /E /NJH /NJS /FP /NP /V | findstr /v "*EXTRA File"
robocopy "libs/glib/%build_platform%/bin/" "%quetoo_folder%/bin/" *.dll /E /NJH /NJS /FP /NP /V | findstr /v "*EXTRA File"
robocopy "libs/sdl_mixer/lib/%build_platform%/" "%quetoo_folder%/bin/" *.dll /E /NJH /NJS /FP /NP /V | findstr /v "*EXTRA File"

robocopy "../../ObjectivelyMVC/ObjectivelyMVC.vs15/libs/sdl/lib/%build_platform%/" "%quetoo_folder%/bin/" *.dll /E /NJH /NJS /FP /NP /V | findstr /v "*EXTRA File"
robocopy "../../ObjectivelyMVC/ObjectivelyMVC.vs15/libs/sdl_image/lib/%build_platform%/" "%quetoo_folder%/bin/" *.dll /E /NJH /NJS /FP /NP /V | findstr /v "*EXTRA File"
robocopy "../../ObjectivelyMVC/ObjectivelyMVC.vs15/libs/sdl_ttf/lib/%build_platform%/" "%quetoo_folder%/bin/" *.dll /E /NJH /NJS /FP /NP /V | findstr /v "*EXTRA File"

GOTO DONE

:INVALID
echo "No build input folder. Gonedy."

:DONE