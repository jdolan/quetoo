IF [%1] == [] GOTO INVALID
IF [%2] == [] GOTO INVALID
IF [%1] == [""] GOTO INVALID
IF [%2] == [""] GOTO INVALID

set quetoo_folder=%~1
set build_name=%~2

call ../ROBO "../../src/client/renderer/shaders" "%quetoo_folder%/lib/default/shaders/" *.glsl

GOTO DONE

:INVALID
echo "No build input folder. Gonedy."

:DONE